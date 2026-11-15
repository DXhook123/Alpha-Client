#include "Camera.h"
#include "../Game.h"
#include "../Classes/LocalPlayer.h"
#include "../../Libs/imgui/imgui.h"
#include "../../Utils/Spdlog.h"
#include <Windows.h>
#include <cmath>
#include <cstring>

namespace Camera {
	namespace {
		constexpr uintptr_t kModuleAddrMin = 0x0000700000000000ull;

		struct FrameData {
			float view[16] = {};
			float proj[16] = {};
			Vec3<float> camPos;
			Vec2<float> fov;
			bool useRefDef = true;
			bool useMvp = false;
			bool valid = false;
		};
		FrameData g_frame;

		bool isReadable(uintptr_t addr, size_t size) {
			if (!addr) return false;
			MEMORY_BASIC_INFORMATION mbi{};
			if (!VirtualQuery(reinterpret_cast<const void*>(addr), &mbi, sizeof(mbi))) return false;
			if (mbi.State != MEM_COMMIT) return false;
			const DWORD p = mbi.Protect & 0xFF;
			const bool ok = p == PAGE_READONLY || p == PAGE_READWRITE || p == PAGE_WRITECOPY ||
				p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
			return ok && size <= (mbi.RegionSize - (addr - reinterpret_cast<uintptr_t>(mbi.BaseAddress)));
		}

		uintptr_t safeReadPtr(uintptr_t addr) {
			if (!isReadable(addr, sizeof(void*))) return 0;
			__try { return *reinterpret_cast<uintptr_t*>(addr); }
			__except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
		}

		bool safeReadFloats(float* src, float* dst, size_t count) {
			if (!isReadable(reinterpret_cast<uintptr_t>(src), sizeof(float) * count)) return false;
			__try {
				std::memcpy(dst, src, sizeof(float) * count);
				return true;
			}
			__except (EXCEPTION_EXECUTE_HANDLER) { return false; }
		}

		bool allFinite(const float* m, int n) {
			for (int i = 0; i < n; ++i)
				if (!std::isfinite(m[i])) return false;
			return true;
		}

		bool looksLikeCamToWorld(const float* m) {
			if (!allFinite(m, 16)) return false;
			const float l0 = m[0] * m[0] + m[1] * m[1] + m[2] * m[2];
			const float l1 = m[4] * m[4] + m[5] * m[5] + m[6] * m[6];
			const float l2 = m[8] * m[8] + m[9] * m[9] + m[10] * m[10];
			if (std::fabs(l0 - 1.f) > 0.01f || std::fabs(l1 - 1.f) > 0.01f || std::fabs(l2 - 1.f) > 0.01f) return false;
			const float t = std::fabs(m[12]) + std::fabs(m[13]) + std::fabs(m[14]);
			return t > 10.f;
		}

		bool looksLikeViewRot(const float* m) {
			if (!allFinite(m, 16)) return false;
			const float l0 = m[0] * m[0] + m[1] * m[1] + m[2] * m[2];
			const float l1 = m[4] * m[4] + m[5] * m[5] + m[6] * m[6];
			const float l2 = m[8] * m[8] + m[9] * m[9] + m[10] * m[10];
			if (std::fabs(l0 - 1.f) > 0.01f || std::fabs(l1 - 1.f) > 0.01f || std::fabs(l2 - 1.f) > 0.01f) return false;
			if (std::fabs(m[12]) > 1e-3f || std::fabs(m[13]) > 1e-3f || std::fabs(m[14]) > 1e-3f) return false;
			int nontrivial = 0;
			const int idx[9] = { 0,1,2,4,5,6,8,9,10 };
			for (int i : idx)
				if (std::fabs(m[i]) > 0.01f && std::fabs(m[i]) < 0.999f) ++nontrivial;
			return nontrivial >= 3;
		}

		void mat4Transpose(const float* m, float* out) {
			for (int i = 0; i < 4; ++i)
				for (int j = 0; j < 4; ++j)
					out[i * 4 + j] = m[j * 4 + i];
		}

		void fixupRelativeView(float* v) {
			const float zn = -v[6];
			const float zt = -v[9];
			if (zn < -0.01f) return;
			if (zt < -0.01f) {
				float t[16];
				mat4Transpose(v, t);
				std::memcpy(v, t, sizeof(t));
				return;
			}
			if (zn > 0.01f || zt > 0.01f) {
				if (zt > 0.01f) {
					float t[16];
					mat4Transpose(v, t);
					std::memcpy(v, t, sizeof(t));
				}
				v[2] = -v[2]; v[6] = -v[6]; v[10] = -v[10]; v[14] = -v[14];
			}
		}

		float currentAspect() {
			const ImVec2 display = ImGui::GetIO().DisplaySize;
			return (display.y > 0.f) ? display.x / display.y : 1.777f;
		}

		float expectedFocalY() {
			const float deg = 70.f;
			return 1.f / std::tan(deg * 0.5f * 3.14159265f / 180.f);
		}

		float fallbackFocalY() {
			constexpr float kFovCalibration = 1.23f;
			const float deg = 70.f;
			return 1.f / (std::tan(deg * 0.5f * 3.14159265f / 180.f) * kFovCalibration);
		}

		bool findFocalPair(uintptr_t obj, uintptr_t& cachedOff) {
			const float aspect = currentAspect();
			const float expectFy = expectedFocalY();
			float buf[0x100];
			if (cachedOff) {
				float pair[2];
				if (safeReadFloats(reinterpret_cast<float*>(obj + cachedOff), pair, 2)
					&& pair[0] > 0.1f && pair[0] < 3.f && pair[1] > 0.1f && pair[1] < 3.f)
					return true;
				cachedOff = 0;
			}
			if (!safeReadFloats(reinterpret_cast<float*>(obj + 0x800), buf, 0x100)) return false;
			for (int i = 0; i + 1 < 0x100; ++i) {
				const float a = buf[i], b = buf[i + 1];
				if (a < 0.1f || a > 3.f || b < 0.1f || b > 3.f) continue;
				const float r = b / a;
				const bool ratioOk = std::fabs(r - 1.f / aspect) < 0.03f || std::fabs(r - aspect) < 0.03f;
				if (!ratioOk) continue;
				const float fy = (std::fabs(r - 1.f / aspect) < 0.03f) ? a : b;
				if (std::fabs(fy - expectFy) > expectFy * 0.3f) continue;
				cachedOff = 0x800 + i * 4;
				return true;
			}
			return false;
		}

		bool readLrpFocal(uintptr_t lrp, Vec2<float>& outFov) {
			float pair[2] = { 0.f, 0.f };
			if (!safeReadFloats(reinterpret_cast<float*>(lrp + 0xF58), pair, 2)) return false;
			if (!std::isfinite(pair[0]) || !std::isfinite(pair[1])) return false;
			if (pair[0] < 0.1f || pair[0] > 10.f || pair[1] < 0.1f || pair[1] > 10.f) return false;
			const float aspect = currentAspect();
			if (std::fabs(pair[0] * aspect - pair[1]) < 0.15f * pair[1]) {
				outFov = Vec2<float>(pair[0], pair[1]);
				return true;
			}
			if (std::fabs(pair[1] * aspect - pair[0]) < 0.15f * pair[0]) {
				outFov = Vec2<float>(pair[1], pair[0]);
				return true;
			}
			return false;
		}

		uintptr_t g_renderObj = 0;
		uintptr_t g_fovOff = 0;
		bool g_mainFailed = false;
		bool g_logged = false;

		uintptr_t g_latiteLrp = 0;
		uintptr_t g_latiteGameRenderer = 0;
		bool g_latiteLogged = false;

		bool latiteMatricesPlausible(const float* view, const float* proj) {
			if (!allFinite(view, 16) || !allFinite(proj, 16)) return false;
			if (std::fabs(view[12]) > 1.f || std::fabs(view[13]) > 1.f || std::fabs(view[14]) > 1.f) return false;
			if (std::fabs(proj[0]) < 0.05f || std::fabs(proj[0]) > 10.f) return false;
			if (std::fabs(proj[5]) < 0.05f || std::fabs(proj[5]) > 10.f) return false;
			return true;
		}

		bool tryCaptureLatite(ClientInstance* client) {
			const auto& layout = SdkVersion::current();
			if (SdkVersion::is12651()) {
				g_latiteLrp = 0;
				g_latiteGameRenderer = 0;
			}
			if (!g_latiteLrp) {
				const uintptr_t lr = safeReadPtr(reinterpret_cast<uintptr_t>(client) + layout.levelRenderer);
				if (!lr || lr >= kModuleAddrMin) return false;
				g_latiteLrp = safeReadPtr(lr + layout.renderPlayer);
				if (!g_latiteLrp || g_latiteLrp >= kModuleAddrMin) {
					g_latiteLrp = 0;
					return false;
				}
			}

			if (!g_latiteGameRenderer) {
				const uintptr_t mcGame = safeReadPtr(reinterpret_cast<uintptr_t>(client) + layout.mcGame);
				if (!mcGame || mcGame >= kModuleAddrMin) return false;
				g_latiteGameRenderer = SdkVersion::is12651()
					? MemoryUtil::callVirtualFunc<uintptr_t>(layout.getGameRenderer, client)
					: safeReadPtr(mcGame + layout.gameRenderer);
				if (!g_latiteGameRenderer || g_latiteGameRenderer >= kModuleAddrMin) {
					g_latiteGameRenderer = 0;
					return false;
				}
			}

			float origin[3], view[16], proj[16];
			if (!safeReadFloats(reinterpret_cast<float*>(g_latiteLrp + layout.cameraPos), origin, 3)) return false;
			if (!safeReadFloats(reinterpret_cast<float*>(g_latiteGameRenderer + layout.viewMatrix), view, 16)) return false;
			if (!safeReadFloats(reinterpret_cast<float*>(g_latiteGameRenderer + layout.projectionMatrix), proj, 16)) return false;
			if (!latiteMatricesPlausible(view, proj)) return false;
			if (!std::isfinite(origin[0]) || !std::isfinite(origin[1]) || !std::isfinite(origin[2])) return false;
			if (!SdkVersion::is12651() && std::fabs(origin[0]) + std::fabs(origin[1]) + std::fabs(origin[2]) < 10.f) return false;

			std::memcpy(g_frame.view, view, sizeof(view));
			std::memcpy(g_frame.proj, proj, sizeof(proj));
			g_frame.camPos = Vec3<float>(origin[0], origin[1], origin[2]);
			g_frame.useMvp = true;
			g_frame.useRefDef = false;
			g_frame.valid = true;
			Vec2<float> lrpFocal;
			if (SdkVersion::is12651())
				g_frame.fov = Vec2<float>(proj[0], proj[5]);
			else if (readLrpFocal(g_latiteLrp, lrpFocal))
				g_frame.fov = lrpFocal;
			if (!g_latiteLogged) {
				float raw[2] = { 0.f, 0.f };
				if (!SdkVersion::is12651()) safeReadFloats(reinterpret_cast<float*>(g_latiteLrp + 0xF58), raw, 2);
				LOG_INFO("[Camera] Latite 主路径就绪: LRP=%p GR=%p | LRP focal=(%.4f,%.4f) vs proj=(%.4f,%.4f)",
					(void*)g_latiteLrp, (void*)g_latiteGameRenderer, raw[0], raw[1], proj[0], proj[5]);
				g_latiteLogged = true;
			}
			return true;
		}
	}

	void captureFrame() {
		g_frame.valid = false;
		g_frame.useRefDef = true;
		g_frame.useMvp = false;

		ClientInstance* client = Game::clientInstance;
		if (!client || !isReadable(reinterpret_cast<uintptr_t>(client), 0x1400)) return;
		const uintptr_t ci = reinterpret_cast<uintptr_t>(client);

		if (tryCaptureLatite(client)) return;
		if (SdkVersion::is12651()) return;

		if (!g_renderObj) {
			const uintptr_t lr = safeReadPtr(ci + 0x1B8);
			if (lr && lr < kModuleAddrMin) {
				for (uintptr_t off : { 0x468u, 0x470u }) {
					const uintptr_t obj = safeReadPtr(lr + off);
					if (!obj || obj >= kModuleAddrMin) continue;
					float m[16];
					if (safeReadFloats(reinterpret_cast<float*>(obj + 0x800), m, 16) && looksLikeCamToWorld(m)) {
						g_renderObj = obj;
						break;
					}
				}
			}
		}

		float m[16];
		if (g_renderObj && safeReadFloats(reinterpret_cast<float*>(g_renderObj + 0x800), m, 16) && looksLikeCamToWorld(m)) {
			g_frame.camPos = Vec3<float>(m[12], m[13], m[14]);
			for (int i = 0; i < 4; ++i)
				for (int j = 0; j < 4; ++j)
					g_frame.view[i * 4 + j] = (j == 3) ? 0.f : m[j * 4 + i];

			if (!readLrpFocal(g_renderObj, g_frame.fov)) {
				if (findFocalPair(g_renderObj, g_fovOff)) {
					float pair[2];
					safeReadFloats(reinterpret_cast<float*>(g_renderObj + g_fovOff), pair, 2);
					const float aspect = currentAspect();
					const float r = pair[1] / pair[0];
					if (std::fabs(r - 1.f / aspect) < 0.05f)
						g_frame.fov = Vec2<float>(pair[1], pair[0]);
					else if (std::fabs(r - aspect) < 0.05f)
						g_frame.fov = Vec2<float>(pair[0], pair[1]);
					else
						g_frame.fov = Vec2<float>(pair[1], pair[0]);
				}
				else {
					const float fy = fallbackFocalY();
					g_frame.fov = Vec2<float>(fy / currentAspect(), fy);
				}
			}

			g_frame.valid = true;
			if (!g_logged) {
				LOG_INFO("[Camera] 主路径就绪: renderObj=%p viewProj@0x800 fov=(%.3f,%.3f)", (void*)g_renderObj,
					g_frame.fov.x, g_frame.fov.y);
				g_logged = true;
			}
			return;
		}

		float v[16];
		if (safeReadFloats(reinterpret_cast<float*>(ci + 0x418), v, 16) && looksLikeViewRot(v)) {
			fixupRelativeView(v);
			std::memcpy(g_frame.view, v, sizeof(v));
			if (LocalPlayer* lp = Game::getLocalPlayer()) {
				__try {
					Vec3<float> p = lp->getPos();
					g_frame.camPos = Vec3<float>(p.x, p.y + 1.62f, p.z);
				}
				__except (EXCEPTION_EXECUTE_HANDLER) {
					return;
				}
			}
			else return;
			uintptr_t lrpFov = 0;
			const uintptr_t lrB = safeReadPtr(ci + 0x1B8);
			if (lrB && lrB < kModuleAddrMin)
				lrpFov = safeReadPtr(lrB + 0x468);
			if (!lrpFov || !readLrpFocal(lrpFov, g_frame.fov)) {
				const float fy = fallbackFocalY();
				g_frame.fov = Vec2<float>(fy / currentAspect(), fy);
			}
			g_frame.valid = true;
			if (!g_mainFailed) {
				LOG_INFO("[Camera] 主路径未命中，使用备路径 CI+0x418（相机位置=玩家眼高估算）");
				g_mainFailed = true;
			}
			return;
		}
	}

	bool worldToScreen(const Vec3<float>& worldPos, Vec2<float>& outScreen) {
		if (!g_frame.valid) return false;
		const ImVec2 display = ImGui::GetIO().DisplaySize;
		if (display.x <= 0.f || display.y <= 0.f) return false;

		if (g_frame.useMvp) {
			const float rx = worldPos.x - g_frame.camPos.x;
			const float ry = worldPos.y - g_frame.camPos.y;
			const float rz = worldPos.z - g_frame.camPos.z;
			const float* v = g_frame.view;
			const float* p = g_frame.proj;

			const float ex = v[0] * rx + v[4] * ry + v[8] * rz + v[12];
			const float ey = v[1] * rx + v[5] * ry + v[9] * rz + v[13];
			const float ez = v[2] * rx + v[6] * ry + v[10] * rz + v[14];
			const float ew = v[3] * rx + v[7] * ry + v[11] * rz + v[15];

			const float cx = p[0] * ex + p[4] * ey + p[8] * ez + p[12] * ew;
			const float cy = p[1] * ex + p[5] * ey + p[9] * ez + p[13] * ew;
			const float cw = p[3] * ex + p[7] * ey + p[11] * ez + p[15] * ew;
			if (cw < 0.1f) return false;
			if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(cw)) return false;

			const float ndcX = cx / cw;
			const float ndcY = cy / cw;
			outScreen.x = (ndcX + 1.0f) * 0.5f * display.x;
			outScreen.y = (1.0f - ndcY) * 0.5f * display.y;
			return std::isfinite(outScreen.x) && std::isfinite(outScreen.y);
		}

		const float rx = worldPos.x - g_frame.camPos.x;
		const float ry = worldPos.y - g_frame.camPos.y;
		const float rz = worldPos.z - g_frame.camPos.z;
		const float* m = g_frame.view;

		const float x = m[0] * rx + m[4] * ry + m[8] * rz + m[12];
		const float y = m[1] * rx + m[5] * ry + m[9] * rz + m[13];
		const float z = m[2] * rx + m[6] * ry + m[10] * rz + m[14];
		if (z > -0.01f) return false;
		if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return false;

		const float midX = display.x * 0.5f;
		const float midY = display.y * 0.5f;
		outScreen.x = midX + (midX * x / -z * g_frame.fov.x);
		outScreen.y = midY - (midY * y / -z * g_frame.fov.y);
		return std::isfinite(outScreen.x) && std::isfinite(outScreen.y);
	}

	Vec3<float> getCameraPos() {
		return g_frame.camPos;
	}

	Vec2<float> getFov() {
		if (g_frame.valid && g_frame.useMvp && g_frame.proj[0] > 0.f && g_frame.proj[5] > 0.f)
			return Vec2<float>(g_frame.proj[0], g_frame.proj[5]);
		return g_frame.fov;
	}

	bool getViewBasis(Vec3<float>& right, Vec3<float>& up, Vec3<float>& forward) {
		if (!g_frame.valid) return false;
		const float* m = g_frame.view;
		right = Vec3<float>(m[0], m[4], m[8]);
		up = Vec3<float>(m[1], m[5], m[9]);
		forward = Vec3<float>(-m[2], -m[6], -m[10]);
		return std::isfinite(right.x) && std::isfinite(right.y) && std::isfinite(right.z) &&
			std::isfinite(up.x) && std::isfinite(up.y) && std::isfinite(up.z) &&
			std::isfinite(forward.x) && std::isfinite(forward.y) && std::isfinite(forward.z);
	}
}
