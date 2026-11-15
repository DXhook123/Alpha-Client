#include "../Client/Managers/HookManager/Hooks/Game/GetGammaHook.h"
#include "DrawUtil.h"
#include "../sdk/Game.h"
#include "../sdk/Render/LevelRenderer.h"
#include "../Client/Managers/ModuleManager/ModuleManager.h"
#include "../Utils/Spdlog.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace DrawUtil {

	FrameTransform frame;

	namespace {
		constexpr uintptr_t kModuleAddrMin = 0x0000700000000000ull;

		bool isReadable(uintptr_t addr, size_t size) {
			if (addr == 0) return false;
			MEMORY_BASIC_INFORMATION mbi{};
			if (VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi)) == 0) return false;
			if (mbi.State != MEM_COMMIT) return false;
			const DWORD protect = mbi.Protect & 0xFF;
			const bool readable = protect == PAGE_READONLY || protect == PAGE_READWRITE ||
				protect == PAGE_WRITECOPY || protect == PAGE_EXECUTE_READ ||
				protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
			return readable && size <= (mbi.RegionSize - (addr - reinterpret_cast<uintptr_t>(mbi.BaseAddress)));
		}

		bool safeReadFloats(float* src, float* dst, size_t count) {
			if (!isReadable(reinterpret_cast<uintptr_t>(src), sizeof(float) * count)) return false;
			__try {
				std::memcpy(dst, src, sizeof(float) * count);
				return true;
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {
				return false;
			}
		}

		uintptr_t safeReadPtr(uintptr_t addr) {
			if (!isReadable(addr, sizeof(void*))) return 0;
			__try {
				return *reinterpret_cast<uintptr_t*>(addr);
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {
				return 0;
			}
		}

		bool safeGetEyePos(LocalPlayer* lp, Vec3<float>& out) {
			__try {
				Vec3<float> pos = lp->getPos();
				out = Vec3<float>(pos.x, pos.y + 1.62f, pos.z);
				return true;
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {
				return false;
			}
		}

		void mat4Mul(const float* a, const float* b, float* out) {
			for (int col = 0; col < 4; ++col) {
				for (int row = 0; row < 4; ++row) {
					float sum = 0.f;
					for (int k = 0; k < 4; ++k)
						sum += a[k * 4 + row] * b[col * 4 + k];
					out[col * 4 + row] = sum;
				}
			}
		}

		void mat4Transpose(const float* m, float* out) {
			for (int i = 0; i < 4; ++i)
				for (int j = 0; j < 4; ++j)
					out[i * 4 + j] = m[j * 4 + i];
		}

		bool allFinite(const float* m) {
			for (int i = 0; i < 16; ++i)
				if (!std::isfinite(m[i])) return false;
			return true;
		}

		bool matrixLooksSane(const float* m) {
			int smallCount = 0;
			for (int i = 0; i < 16; ++i) {
				if (!std::isfinite(m[i])) return false;
				if (std::fabs(m[i]) < 8.f) ++smallCount;
			}
			return smallCount >= 14;
		}

		void dumpObject(uintptr_t base, size_t size, std::vector<float>& out) {
			out.assign(size / 4, 0.f);
			for (size_t off = 0; off < size; off += 0x1000) {
				const size_t chunk = (std::min)((size_t)0x1000, size - off);
				safeReadFloats(reinterpret_cast<float*>(base + off), out.data() + off / 4, chunk / 4);
			}
		}

		int findVec3Near(const std::vector<float>& buf, const Vec3<float>& ref) {
			for (size_t i = 0; i + 3 <= buf.size(); ++i) {
				if (!std::isfinite(buf[i]) || !std::isfinite(buf[i + 1]) || !std::isfinite(buf[i + 2])) continue;
				if (std::fabs(buf[i] - ref.x) < 32.f && std::fabs(buf[i + 1] - ref.y) < 16.f && std::fabs(buf[i + 2] - ref.z) < 32.f)
					return (int)(i * 4);
			}
			return -1;
		}

		int classifyCanonical(const float* m) {
			if (!allFinite(m)) return -1;

			if (m[3] == 0.f && m[7] == 0.f && m[15] == 0.f && (m[11] == 1.f || m[11] == -1.f)) {
				if (!(m[0] > 0.05f && m[0] < 20.f && m[5] > 0.05f && m[5] < 20.f)) return -1;

				int nontrivial = 0;
				const int rotIdx[6] = { 1,2,4,6,8,9 };
				for (int idx : rotIdx)
					if (std::fabs(m[idx]) > 0.01f && std::fabs(m[idx]) < 0.999f) ++nontrivial;

				const bool hasTrans = std::fabs(m[12]) > 1e-3f || std::fabs(m[13]) > 1e-3f || std::fabs(m[14]) > 1e-3f;
				if (hasTrans) {
					const float tmag = (std::max)({ std::fabs(m[12]), std::fabs(m[13]), std::fabs(m[14]) });
					if (tmag > 10.f && nontrivial >= 2) return 2;
					return -1;
				}

				const bool diag = std::fabs(m[1]) < 1e-3f && std::fabs(m[2]) < 1e-3f && std::fabs(m[4]) < 1e-3f &&
					std::fabs(m[6]) < 1e-3f && std::fabs(m[8]) < 1e-3f && std::fabs(m[9]) < 1e-3f;
				if (diag) {
					const bool nearOk = std::fabs(m[14]) > 1e-4f;
					const bool unitLike = m[0] == 1.f && m[5] == 1.f;
					return (nearOk && !unitLike) ? 0 : -1;
				}
				return nontrivial >= 3 ? 3 : -1;
			}

			const float l0 = m[0] * m[0] + m[1] * m[1] + m[2] * m[2];
			const float l1 = m[4] * m[4] + m[5] * m[5] + m[6] * m[6];
			const float l2 = m[8] * m[8] + m[9] * m[9] + m[10] * m[10];
			if (std::fabs(l0 - 1.f) < 0.01f && std::fabs(l1 - 1.f) < 0.01f && std::fabs(l2 - 1.f) < 0.01f
				&& m[3] == 0.f && m[7] == 0.f && m[11] == 0.f && m[15] == 1.f) {
				int nontrivial = 0;
				const int rotIdx[9] = { 0,1,2,4,5,6,8,9,10 };
				for (int idx : rotIdx)
					if (std::fabs(m[idx]) > 0.01f && std::fabs(m[idx]) < 0.999f) ++nontrivial;
				if (nontrivial >= 3) return 1;
			}
			return -1;
		}

		int classifyMatrixEx(const float* m, bool& transposed) {
			transposed = false;
			int kind = classifyCanonical(m);
			if (kind >= 0) return kind;
			float t[16];
			mat4Transpose(m, t);
			kind = classifyCanonical(t);
			if (kind >= 0) {
				transposed = true;
				return kind;
			}
			return -1;
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

		float readGameFovDegrees() {
			const uintptr_t opt = reinterpret_cast<uintptr_t>(gCapturedOptions);
			if (!opt || !isReadable(opt, 0x200)) return 0.f;
			const uintptr_t fo = safeReadPtr(opt + 0x1A0);
			if (!fo || fo >= kModuleAddrMin || !isReadable(fo, 0x20)) return 0.f;
			float v = 0.f;
			if (!safeReadFloats(reinterpret_cast<float*>(fo + 0x18), &v, 1)) return 0.f;
			return (v >= 30.f && v <= 110.f) ? v : 0.f;
		}

		Vec2<float> makeRefDefFovParams() {
			constexpr float kFovCalibration = 1.23f;
			const float fovDeg = readGameFovDegrees();
			const float deg = fovDeg > 0.f ? fovDeg : 70.f;
			const ImVec2 display = ImGui::GetIO().DisplaySize;
			const float aspect = (display.y > 0.f) ? display.x / display.y : 1.777f;
			const float fy = 1.f / (std::tan(deg * 0.5f * 3.14159265f / 180.f) * kFovCalibration);
			return Vec2<float>(fy / aspect, fy);
		}

		float currentAspect() {
			const ImVec2 display = ImGui::GetIO().DisplaySize;
			return (display.y > 0.f) ? display.x / display.y : 1.777f;
		}
		int findFocalPair(const std::vector<float>& buf, float aspect, float expectFy) {
			if (expectFy <= 0.f) return -1;
			for (size_t i = 0; i + 1 < buf.size(); ++i) {
				const float a = buf[i], b = buf[i + 1];
				if (a < 0.1f || a > 3.f || b < 0.1f || b > 3.f) continue;
				const float r = b / a;
				const bool ratioOk = std::fabs(r - 1.f / aspect) < 0.03f || std::fabs(r - aspect) < 0.03f;
				if (!ratioOk) continue;
				const float fy = (std::fabs(b / a - 1.f / aspect) < 0.03f) ? a : b;
				if (std::fabs(fy - expectFy) > expectFy * 0.3f) continue;
				return (int)(i * 4);
			}
			return -1;
		}

		struct AdoptParams {
			uintptr_t ciOff = 0, midOff = 0, grandOff = 0;
			uintptr_t camOff = 0;
			uintptr_t viewOff = 0;
			uintptr_t projOff = 0;
			uintptr_t matOff = 0;
			int mode = -1;
			bool transView = false, transProj = false, transMat = false;
			bool viewInCi = false;
			uintptr_t fovOff = 0;
		};
		AdoptParams sAdopt;
		bool sAdoptValid = false;
		bool sDiagDone = false;

		bool loadMatrix(uintptr_t obj, uintptr_t off, bool transpose, int expectKind, float* out) {
			float tmp[16];
			if (!safeReadFloats(reinterpret_cast<float*>(obj + off), tmp, 16)) return false;
			if (transpose) {
				float t[16];
				mat4Transpose(tmp, t);
				std::memcpy(tmp, t, sizeof(tmp));
			}
			if (classifyCanonical(tmp) != expectKind) return false;
			std::memcpy(out, tmp, sizeof(tmp));
			return true;
		}

		bool captureViaParams(ClientInstance* client, const AdoptParams& ap) {
			const uintptr_t ci = reinterpret_cast<uintptr_t>(client);
			uintptr_t p = ap.ciOff ? safeReadPtr(ci + ap.ciOff) : ci;
			if (!p || p >= kModuleAddrMin || !isReadable(p, 0x100)) return false;
			uintptr_t mid = ap.midOff ? safeReadPtr(p + ap.midOff) : p;
			if (!mid || mid >= kModuleAddrMin || !isReadable(mid, 0x100)) return false;
			uintptr_t obj = ap.grandOff ? safeReadPtr(mid + ap.grandOff) : mid;
			if (!obj || obj >= kModuleAddrMin || !isReadable(obj, 0x1000)) return false;

			if (ap.camOff) {
				float cam[3]{};
				if (safeReadFloats(reinterpret_cast<float*>(obj + ap.camOff), cam, 3))
					frame.cameraPos = Vec3<float>(cam[0], cam[1], cam[2]);
			}

			if (ap.mode == 0 || ap.mode == 3) {
				const uintptr_t viewObj = ap.viewInCi ? ci : obj;
				float view[16]{}, proj[16]{};
				if (!loadMatrix(viewObj, ap.viewOff, ap.transView, 1, view)) return false;
				if (ap.mode == 0)
					fixupRelativeView(view);
				if (!loadMatrix(obj, ap.projOff, ap.transProj, 0, proj)) return false;
				mat4Mul(proj, view, frame.viewProjection);
				if (!allFinite(frame.viewProjection)) return false;
				frame.fov = Vec2<float>(proj[0], proj[5]);
				frame.useRefDef = false;
			}
			else if (ap.mode == 1) {
				if (!loadMatrix(obj, ap.matOff, ap.transMat, 2, frame.viewProjection)) return false;
				frame.fov = Vec2<float>(frame.viewProjection[0], frame.viewProjection[5]);
				frame.useRefDef = false;
			}
			else if (ap.mode == 2) {
				if (!loadMatrix(obj, ap.matOff, ap.transMat, 3, frame.viewProjection)) return false;
				frame.fov = Vec2<float>(frame.viewProjection[0], frame.viewProjection[5]);
				frame.useRefDef = false;
			}
			else if (ap.mode == 4) {
				if (!loadMatrix(ci, ap.viewOff, ap.transView, 1, frame.viewProjection)) return false;
				fixupRelativeView(frame.viewProjection);
				frame.fov = makeRefDefFovParams();
				frame.useRefDef = true;
			}
			else if (ap.mode == 5) {
				float m[16];
				if (!loadMatrix(obj, ap.viewOff, ap.transView, 1, m)) return false;
				frame.cameraPos = Vec3<float>(m[12], m[13], m[14]);
				float v[16];
				for (int i = 0; i < 4; ++i)
					for (int j = 0; j < 4; ++j)
						v[i * 4 + j] = (j == 3) ? 0.f : m[j * 4 + i];
				std::memcpy(frame.viewProjection, v, sizeof(v));

				bool gotFov = false;
				if (ap.fovOff) {
					float pair[2]{};
					if (safeReadFloats(reinterpret_cast<float*>(obj + ap.fovOff), pair, 2)
						&& pair[0] > 0.05f && pair[0] < 20.f && pair[1] > 0.05f && pair[1] < 20.f) {
						const float aspect = currentAspect();
						const float r = pair[1] / pair[0];
						if (std::fabs(r - 1.f / aspect) < 0.05f)
							frame.fov = Vec2<float>(pair[1], pair[0]);
						else if (std::fabs(r - aspect) < 0.05f)
							frame.fov = Vec2<float>(pair[0], pair[1]);
						else
							frame.fov = Vec2<float>(pair[1], pair[0]);
						gotFov = true;
					}
				}
				if (!gotFov)
					frame.fov = makeRefDefFovParams();
				frame.useRefDef = true;
			}
			else return false;

			frame.matrixMode = ap.mode;
			frame.valid = true;
			return true;
		}

		struct Cand { int off = -1; bool trans = false; };
		struct ScanCtx {
			Vec3<float> ref;
			int dumps = 0;
			int dumpBudget = 500;
			std::vector<uintptr_t> vec3OnlyObjs;
			Cand ciView;
			bool adopted = false;
		};

		bool tryAdoptObject(ScanCtx& ctx, uintptr_t obj, uintptr_t ciOff, uintptr_t midOff, uintptr_t grandOff) {
			if (ctx.adopted || obj == 0 || obj >= kModuleAddrMin || !isReadable(obj, 0x1000)) return false;
			if (++ctx.dumps > ctx.dumpBudget) return false;

			std::vector<float> buf;
			dumpObject(obj, 0x1000, buf);

			Cand firstView, firstViewAbs, firstProj, firstVpAbs, firstVpRel;
			for (size_t i = 0; i + 16 <= buf.size(); ++i) {
				bool trans = false;
				const int kind = classifyMatrixEx(buf.data() + i, trans);
				if (kind < 0) continue;
				Cand c{ (int)(i * 4), trans };
				if (kind == 0 && firstProj.off < 0) firstProj = c;
				else if (kind == 1 && firstView.off < 0) firstView = c;
				else if (kind == 2 && firstVpAbs.off < 0) firstVpAbs = c;
				else if (kind == 3 && firstVpRel.off < 0) firstVpRel = c;
			}

			if (firstView.off >= 0) {
				const float* vm = buf.data() + firstView.off / 4;
				const bool absView = std::fabs(vm[12]) > 10.f || std::fabs(vm[13]) > 10.f || std::fabs(vm[14]) > 10.f;
				if (absView) firstViewAbs = firstView;
			}

			if (obj == reinterpret_cast<uintptr_t>(Game::clientInstance) && firstView.off >= 0 && firstViewAbs.off < 0)
				ctx.ciView = firstView;

			const int camOff = findVec3Near(buf, ctx.ref);

			if (firstVpAbs.off >= 0) {
				sAdopt = { ciOff, midOff, grandOff, (uintptr_t)(camOff > 0 ? camOff : 0),
					0, 0, (uintptr_t)firstVpAbs.off, 1, false, false, firstVpAbs.trans, false };
				ctx.adopted = true;
			}
			else if (firstVpRel.off >= 0) {
				sAdopt = { ciOff, midOff, grandOff, (uintptr_t)(camOff > 0 ? camOff : 0),
					0, 0, (uintptr_t)firstVpRel.off, 2, false, false, firstVpRel.trans, false };
				ctx.adopted = true;
			}
			else if (firstViewAbs.off >= 0) {
				sAdopt = { ciOff, midOff, grandOff, (uintptr_t)(camOff > 0 ? camOff : 0),
					(uintptr_t)firstViewAbs.off, 0, 0, 5, firstViewAbs.trans, false, false, false, 0 };
				{
					const float fovDeg = readGameFovDegrees();
					const float expectFy = 1.f / std::tan((fovDeg > 0.f ? fovDeg : 70.f) * 0.5f * 3.14159265f / 180.f);
					const int fo = findFocalPair(buf, currentAspect(), expectFy);
					if (fo >= 0)
						sAdopt.fovOff = (uintptr_t)fo;
				}
				ctx.adopted = true;
			}
			else if (firstView.off >= 0 && firstProj.off >= 0) {
				const float* vm = buf.data() + firstView.off / 4;
				const bool absView = std::fabs(vm[12]) > 10.f || std::fabs(vm[13]) > 10.f || std::fabs(vm[14]) > 10.f;
				sAdopt = { ciOff, midOff, grandOff, (uintptr_t)(camOff > 0 ? camOff : 0),
					(uintptr_t)firstView.off, (uintptr_t)firstProj.off, 0,
					absView ? 3 : 0, firstView.trans, firstProj.trans, false, false };
				ctx.adopted = true;
			}
			else if (ctx.ciView.off >= 0 && firstProj.off >= 0) {
				sAdopt = { ciOff, midOff, grandOff, (uintptr_t)(camOff > 0 ? camOff : 0),
					(uintptr_t)ctx.ciView.off, (uintptr_t)firstProj.off, 0,
					0, ctx.ciView.trans, firstProj.trans, false, true };
				ctx.adopted = true;
			}

			if (ctx.adopted) {
				sAdoptValid = true;
				return true;
			}

			if (camOff >= 0 && ctx.vec3OnlyObjs.size() < 48)
				ctx.vec3OnlyObjs.push_back(obj);
			return false;
		}

		void runOffsetDiagnostics(ClientInstance* client) {
			if (sDiagDone) return;
			if (!Game::getLocalPlayer()) return;
			sDiagDone = true;

			const uintptr_t ci = reinterpret_cast<uintptr_t>(client);

			ScanCtx ctx;
			ctx.ref = frame.cameraPos;

			tryAdoptObject(ctx, ci, 0, 0, 0);

			const auto& layout = SdkVersion::current();
			const uintptr_t priority[] = { layout.levelRenderer, layout.mcGame };
			for (uintptr_t ciOff : priority) {
				if (ctx.adopted || ctx.dumps > ctx.dumpBudget) break;
				uintptr_t p = safeReadPtr(ci + ciOff);
				if (!p || p == ci || p >= kModuleAddrMin || !isReadable(p, 0x1000)) continue;
				tryAdoptObject(ctx, p, ciOff, 0, 0);
				for (uintptr_t midOff = 0x08; midOff <= 0x1000 && !ctx.adopted && ctx.dumps <= ctx.dumpBudget; midOff += 8) {
					uintptr_t obj = safeReadPtr(p + midOff);
					if (!obj || obj == ci || obj == p || obj >= kModuleAddrMin || !isReadable(obj, 0x1000)) continue;
					tryAdoptObject(ctx, obj, ciOff, midOff, 0);
				}
			}
			for (uintptr_t ciOff = 0x08; ciOff <= 0xC00 && !ctx.adopted && ctx.dumps <= ctx.dumpBudget; ciOff += 8) {
				if (ciOff == layout.levelRenderer || ciOff == layout.mcGame) continue;
				uintptr_t p = safeReadPtr(ci + ciOff);
				if (!p || p == ci || p >= kModuleAddrMin || !isReadable(p, 0x1000)) continue;
				tryAdoptObject(ctx, p, ciOff, 0, 0);
				for (uintptr_t midOff = 0x08; midOff <= 0x1000 && !ctx.adopted && ctx.dumps <= ctx.dumpBudget; midOff += 8) {
					uintptr_t obj = safeReadPtr(p + midOff);
					if (!obj || obj == ci || obj == p || obj >= kModuleAddrMin || !isReadable(obj, 0x1000)) continue;
					tryAdoptObject(ctx, obj, ciOff, midOff, 0);
				}
			}

			if (!ctx.adopted) {
				ctx.dumpBudget += 600;
				for (uintptr_t holder : ctx.vec3OnlyObjs) {
					if (ctx.adopted || ctx.dumps > ctx.dumpBudget) break;
					if (!isReadable(holder, 0x100)) continue;
					for (uintptr_t ciOff = 0x08; ciOff <= 0xC00 && !ctx.adopted; ciOff += 8) {
						uintptr_t p = safeReadPtr(ci + ciOff);
						if (!p || !isReadable(p, 8)) continue;
						if (p == holder) { tryAdoptObject(ctx, holder, ciOff, 0, 0); break; }
						for (uintptr_t midOff = 0x08; midOff <= 0x1000 && !ctx.adopted; midOff += 8) {
							if (safeReadPtr(p + midOff) != holder) continue;
							for (uintptr_t gOff = 0x08; gOff <= 0x800 && !ctx.adopted && ctx.dumps <= ctx.dumpBudget; gOff += 8) {
								uintptr_t obj = safeReadPtr(holder + gOff);
								if (!obj || obj == ci || obj == holder || obj >= kModuleAddrMin || !isReadable(obj, 0x1000)) continue;
								tryAdoptObject(ctx, obj, ciOff, midOff, gOff);
							}
							break;
						}
					}
				}
			}

			if (!ctx.adopted && ctx.ciView.off >= 0) {
				sAdopt = AdoptParams{};
				sAdopt.viewOff = (uintptr_t)ctx.ciView.off;
				sAdopt.transView = ctx.ciView.trans;
				sAdopt.mode = 4;
				sAdoptValid = true;
				ctx.adopted = true;
			}
		}

		bool tryCaptureRealMatrices(ClientInstance* client) {
			const uintptr_t ci = reinterpret_cast<uintptr_t>(client);
			const auto& layout = SdkVersion::current();
			const uintptr_t mcGame = safeReadPtr(ci + layout.mcGame);
			if (!mcGame || mcGame >= kModuleAddrMin) return false;
			const uintptr_t gr = SdkVersion::is12651()
				? MemoryUtil::callVirtualFunc<uintptr_t>(layout.getGameRenderer, client)
				: safeReadPtr(mcGame + layout.gameRenderer);
			if (!gr || gr >= kModuleAddrMin || !isReadable(gr, layout.projectionMatrix + 64)) return false;

			float view[16]{}, proj[16]{};
			if (!safeReadFloats(reinterpret_cast<float*>(gr + layout.viewMatrix), view, 16)) return false;
			if (!safeReadFloats(reinterpret_cast<float*>(gr + layout.projectionMatrix), proj, 16)) return false;
			if (!allFinite(view) || !allFinite(proj)) return false;
			if (std::fabs(view[12]) > 1.f || std::fabs(view[13]) > 1.f || std::fabs(view[14]) > 1.f) return false;
			if (proj[0] < 0.05f || proj[0] > 10.f || proj[5] < 0.05f || proj[5] > 10.f) return false;

			const uintptr_t lr = safeReadPtr(ci + layout.levelRenderer);
			if (!lr || lr >= kModuleAddrMin) return false;
			const uintptr_t lrp = safeReadPtr(lr + layout.renderPlayer);
			if (!lrp || lrp >= kModuleAddrMin) return false;
			float origin[3]{};
			if (!safeReadFloats(reinterpret_cast<float*>(lrp + layout.cameraPos), origin, 3)) return false;
			if (!std::isfinite(origin[0]) || !std::isfinite(origin[1]) || !std::isfinite(origin[2])) return false;

			mat4Mul(proj, view, frame.viewProjection);
			if (!allFinite(frame.viewProjection)) return false;
			frame.cameraPos = Vec3<float>(origin[0], origin[1], origin[2]);
			frame.fov = Vec2<float>(proj[0], proj[5]);
			frame.matrixMode = 0;
			frame.useRefDef = false;
			frame.valid = true;

			return true;
		}

		void captureFrameTransform() {
			frame.valid = false;
			frame.useRefDef = false;
			frame.matrixMode = 0;
			frame.fov = Vec2<float>(0.f, 0.f);

			ClientInstance* client = Game::clientInstance;
			if (!client || !isReadable(reinterpret_cast<uintptr_t>(client), 0x800)) return;
			if (LocalPlayer* lp = Game::getLocalPlayer())
				safeGetEyePos(lp, frame.cameraPos);
			if (tryCaptureRealMatrices(client)) return;
			if (SdkVersion::is12651()) return;
			if (!SdkVersion::is12651()) {
				AdoptParams def;
				def.ciOff = 0x1B8; def.midOff = 0x478; def.grandOff = 0;
				def.camOff = 0x710; def.viewOff = 0xFB0; def.projOff = 0xFF0; def.mode = 0;
				if (captureViaParams(client, def)) return;
			}

			if (sAdoptValid) {
				if (captureViaParams(client, sAdopt)) {
					return;
				}
				static int sScanFails = 0;
				if (++sScanFails > 600) {
					sScanFails = 0;
					sAdoptValid = false;
					sAdopt = AdoptParams{};
					sDiagDone = false;
				}
			}
			if (!SdkVersion::is12651()) {
				float fovX = 0.f, fovY = 0.f;
				uintptr_t lr = safeReadPtr(reinterpret_cast<uintptr_t>(client) + 0x1B8);
				if (lr) {
					uintptr_t rp = safeReadPtr(lr + 0x478);
					if (rp) {
						safeReadFloats(reinterpret_cast<float*>(rp + 0xFF0), &fovX, 1);
						safeReadFloats(reinterpret_cast<float*>(rp + 0x1004), &fovY, 1);
					}
				}
				if (fovX > 0.01f && fovY > 0.01f) {
					float raw[16]{};
					if (safeReadFloats(reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(client) + 0x330), raw, 16)
						&& matrixLooksSane(raw)) {
						mat4Transpose(raw, frame.viewProjection);
						frame.fov = Vec2<float>(fovX, fovY);
						frame.useRefDef = true;
						frame.valid = true;
						return;
					}
				}
			}
			if (!SdkVersion::is12651()) {
				uintptr_t mg = safeReadPtr(reinterpret_cast<uintptr_t>(client) + 0x1A0);
				uintptr_t gr = mg ? safeReadPtr(mg + 0xFA0) : 0;
				if (gr && gr < kModuleAddrMin && isReadable(gr, 0x400)) {
					float view[16]{}, proj[16]{};
					if (safeReadFloats(reinterpret_cast<float*>(gr + 0x360), view, 16)
						&& safeReadFloats(reinterpret_cast<float*>(gr + 0x3E0), proj, 16)
						&& matrixLooksSane(view) && matrixLooksSane(proj)) {
						mat4Mul(proj, view, frame.viewProjection);
						if (allFinite(frame.viewProjection)) {
							frame.fov = Vec2<float>(proj[0], proj[5]);
							frame.valid = true;
							return;
						}
					}
				}
			}

			runOffsetDiagnostics(client);
			if (sAdoptValid && captureViaParams(client, sAdopt)) return;
		}
	}

	struct ProjectedPoint {
		Vec2<float> screen;
		float projectedX = 0.f;
		float projectedY = 0.f;
		float depth = 0.f;
	};

	constexpr float kNearClip = 0.01f;

	bool updateProjectedScreen(ProjectedPoint& point, const ImVec2& display) {
		const float safeDepth = std::fabs(point.depth) > 0.0001f
			? point.depth
			: (point.depth < 0.f ? -0.0001f : 0.0001f);
		const float midX = display.x * 0.5f;
		const float midY = display.y * 0.5f;

		if (frame.useRefDef) {
			point.screen.x = midX + (midX * point.projectedX / safeDepth * frame.fov.x);
			point.screen.y = midY - (midY * point.projectedY / safeDepth * frame.fov.y);
		}
		else {
			point.screen.x = (1.f + point.projectedX / safeDepth) * display.x * 0.5f;
			point.screen.y = (1.f - point.projectedY / safeDepth) * display.y * 0.5f;
		}
		return std::isfinite(point.screen.x) && std::isfinite(point.screen.y);
	}

	bool projectWorldPoint(const Vec3<float>& worldPos, ProjectedPoint& out) {
		if (!frame.valid) return false;
		const ImVec2 display = ImGui::GetIO().DisplaySize;
		if (display.x <= 0.f || display.y <= 0.f) return false;

		float rx = worldPos.x, ry = worldPos.y, rz = worldPos.z;
		if (frame.matrixMode == 0 || frame.matrixMode == 2 || frame.matrixMode == 4 || frame.matrixMode == 5) {
			rx -= frame.cameraPos.x;
			ry -= frame.cameraPos.y;
			rz -= frame.cameraPos.z;
		}
		const float* m = frame.viewProjection;

		if (frame.useRefDef) {
			const float x = m[0] * rx + m[4] * ry + m[8] * rz + m[12];
			const float y = m[1] * rx + m[5] * ry + m[9] * rz + m[13];
			const float z = m[2] * rx + m[6] * ry + m[10] * rz + m[14];
			out.projectedX = x;
			out.projectedY = y;
			out.depth = -z;
			if (!std::isfinite(out.projectedX) || !std::isfinite(out.projectedY) || !std::isfinite(out.depth)) return false;
			return updateProjectedScreen(out, display);
		}

		const float clipX = m[0] * rx + m[4] * ry + m[8] * rz + m[12];
		const float clipY = m[1] * rx + m[5] * ry + m[9] * rz + m[13];
		const float clipW = m[3] * rx + m[7] * ry + m[11] * rz + m[15];
		out.projectedX = clipX;
		out.projectedY = clipY;
		out.depth = clipW;
		if (!std::isfinite(out.projectedX) || !std::isfinite(out.projectedY) || !std::isfinite(out.depth)) return false;
		return updateProjectedScreen(out, display);
	}

	ProjectedPoint interpolateProjected(const ProjectedPoint& first, const ProjectedPoint& second, float depth) {
		const float denominator = second.depth - first.depth;
		if (std::fabs(denominator) < 0.000001f)
			return first;
		const float factor = (depth - first.depth) / denominator;
		ProjectedPoint result;
		result.projectedX = first.projectedX + (second.projectedX - first.projectedX) * factor;
		result.projectedY = first.projectedY + (second.projectedY - first.projectedY) * factor;
		result.depth = depth;
		updateProjectedScreen(result, ImGui::GetIO().DisplaySize);
		return result;
	}

	bool clipProjectedSegment(ProjectedPoint& first, ProjectedPoint& second) {
		if (first.depth < kNearClip && second.depth < kNearClip)
			return false;
		if (first.depth < kNearClip)
			first = interpolateProjected(first, second, kNearClip);
		if (second.depth < kNearClip)
			second = interpolateProjected(second, first, kNearClip);
		return true;
	}

	void onImGuiWorldRender() {
		if (!Game::getLocalPlayer() || !Game::getLevel()) {
			frame.valid = false;
			return;
		}
		captureFrameTransform();
		ModuleManager::onLevelRender();
	}

	bool worldToScreen(const Vec3<float>& worldPos, Vec2<float>& outScreen) {
		ProjectedPoint projected;
		if (!projectWorldPoint(worldPos, projected) || projected.depth < kNearClip)
			return false;
		outScreen = projected.screen;
		return true;
	}

	void drawBox3dFilled(const AABB& aabb, const UIColor& fillColor, const UIColor& lineColor, float lineWidth) {
		if (!frame.valid) return;
		ImDrawList* dl = ImGui::GetBackgroundDrawList();
		if (!dl) return;

		const Vec3<float> lo = aabb.lower;
		const Vec3<float> hi = aabb.upper;

		const Vec3<float> v[8] = {
			{lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z},
			{lo.x, hi.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, hi.z}, {hi.x, hi.y, hi.z},
		};

		ProjectedPoint projected[8];
		bool ok[8];
		for (int i = 0; i < 8; ++i)
			ok[i] = projectWorldPoint(v[i], projected[i]);

		auto toImV = [](const Vec2<float>& p) { return ImVec2(p.x, p.y); };
		auto fillFace = [&](int a, int b, int c, int d, ImU32 col) {
			if (!ok[a] || !ok[b] || !ok[c] || !ok[d])
				return;

			const ProjectedPoint source[4] = { projected[a], projected[b], projected[c], projected[d] };
			ProjectedPoint clipped[6];
			int clippedCount = 0;
			for (int i = 0; i < 4; ++i) {
				const ProjectedPoint& current = source[i];
				const ProjectedPoint& next = source[(i + 1) % 4];
				const bool currentInside = current.depth >= kNearClip;
				const bool nextInside = next.depth >= kNearClip;

				if (currentInside)
					clipped[clippedCount++] = current;
				if (currentInside != nextInside)
					clipped[clippedCount++] = interpolateProjected(current, next, kNearClip);
			}

			if (clippedCount < 3)
				return;
			ImVec2 polygon[6];
			for (int i = 0; i < clippedCount; ++i)
				polygon[i] = toImV(clipped[i].screen);
			dl->AddConvexPolyFilled(polygon, clippedCount, col);
		};

		if (fillColor.a > 0) {
			const ImU32 fill = IM_COL32(fillColor.r, fillColor.g, fillColor.b, fillColor.a);
			const Vec3<float>& cam = frame.cameraPos;
			if (cam.y < lo.y) fillFace(0, 2, 3, 1, fill);
			if (cam.y > hi.y) fillFace(4, 5, 7, 6, fill);
			if (cam.x < lo.x) fillFace(0, 4, 6, 2, fill);
			if (cam.x > hi.x) fillFace(1, 3, 7, 5, fill);
			if (cam.z < lo.z) fillFace(0, 1, 5, 4, fill);
			if (cam.z > hi.z) fillFace(2, 6, 7, 3, fill);
		}

		if (lineColor.a > 0) {
			const ImU32 line = IM_COL32(lineColor.r, lineColor.g, lineColor.b, lineColor.a);
			static const int edges[12][2] = {
				{4, 5}, {5, 7}, {7, 6}, {6, 4},
				{0, 1}, {1, 3}, {3, 2}, {2, 0},
				{0, 4}, {1, 5}, {2, 6}, {3, 7},
			};
			for (const auto& e : edges) {
				if (!ok[e[0]] || !ok[e[1]])
					continue;
				ProjectedPoint first = projected[e[0]];
				ProjectedPoint second = projected[e[1]];
				if (!clipProjectedSegment(first, second))
					continue;
				dl->AddLine(toImV(first.screen), toImV(second.screen), line, lineWidth);
			}
		}
	}

	void drawLine3d(const Vec3<float>& start, const Vec3<float>& end, const UIColor& color, float thickness) {
		if (!frame.valid || color.a == 0) return;
		ImDrawList* dl = ImGui::GetBackgroundDrawList();
		if (!dl) return;

		ProjectedPoint startPoint, endPoint;
		if (!projectWorldPoint(start, startPoint) || !projectWorldPoint(end, endPoint)) return;
		if (!clipProjectedSegment(startPoint, endPoint)) return;

		dl->AddLine(ImVec2(startPoint.screen.x, startPoint.screen.y),
			ImVec2(endPoint.screen.x, endPoint.screen.y),
			IM_COL32(color.r, color.g, color.b, color.a), thickness);
	}

	void drawPolygon3dFilled(const Vec3<float>* vertices, int count, const UIColor& color) {
		if (!frame.valid || !vertices || count < 3 || count > 8 || color.a == 0) return;
		ImDrawList* dl = ImGui::GetBackgroundDrawList();
		if (!dl) return;
		ProjectedPoint source[8];
		for (int i = 0; i < count; ++i)
			if (!projectWorldPoint(vertices[i], source[i])) return;
		ProjectedPoint clipped[10];
		int clippedCount = 0;
		for (int i = 0; i < count; ++i) {
			const ProjectedPoint& current = source[i];
			const ProjectedPoint& next = source[(i + 1) % count];
			const bool inside = current.depth >= kNearClip;
			const bool nextInside = next.depth >= kNearClip;
			if (inside) clipped[clippedCount++] = current;
			if (inside != nextInside)
				clipped[clippedCount++] = interpolateProjected(current, next, kNearClip);
		}
		if (clippedCount < 3) return;
		ImVec2 polygon[10];
		for (int i = 0; i < clippedCount; ++i)
			polygon[i] = ImVec2(clipped[i].screen.x, clipped[i].screen.y);
		dl->AddConvexPolyFilled(polygon, clippedCount,
			IM_COL32(color.r, color.g, color.b, color.a));
	}
}
