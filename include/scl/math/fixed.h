#pragma once

#include <initializer_list>
#include <cmath>
#include <numbers>
#include <cstdio>
#include <format>

namespace scl {
namespace math {

// --------------------------------------------------------------------------@/
// fixed-point
// --------------------------------------------------------------------------@/
uint32_t sqrti(uint64_t a_nInput) {
	// we just changed the types to uint64_t and changed one from "1uL << 30".
	uint64_t op  = a_nInput;
	uint64_t res = 0;
	uint64_t one = 1ULL << 62; // The second-to-top bit is set: use 1u << 14 for uint16_t type; use 1uL<<30 for uint32_t type

	// "one" starts at the highest power of four <= than the argument.
	while (one > op) {
		one >>= 2;
	}

	while (one != 0)
	{
		if (op >= res + one) {
			op = op - (res + one);
			res = res +  2 * one;
		}
		res >>= 1;
		one >>= 2;
	}

	// optional rounding --------------------------------@/
	if(op > res) {
		res++;
	}

	return res;
}

template<std::size_t S = 12> class QFxi {
	public:
		using value_type = int32_t;
		using real_type = double;
		static constexpr size_t Shift = S;
		static constexpr int One = (1 << Shift);
		static constexpr size_t MaskFrac = One - 1;
		static constexpr size_t MaskWhole = ~MaskFrac;
		static constexpr size_t NumBits = 8 * sizeof(value_type);
		static constexpr int64_t Max = 1LL << (NumBits-1-Shift);
	public:
		value_type mData;

		constexpr QFxi() : mData(0) {}
		constexpr QFxi(value_type v, int shift) : mData(v >> shift) {}
		constexpr QFxi(int v) : mData(v * One) {}
		constexpr QFxi(const float v) : mData(v * One) {}
		constexpr QFxi(const double v) : mData(v * One) {}

		// conversion methods ---------------------------@/
		constexpr auto raw() const -> value_type {
			return mData;
		}
		constexpr auto partFrac() const -> value_type {
			return mData & MaskFrac;
		}
		constexpr auto partWhole() const -> value_type {
			// NOTE: seems to be faster than "mData / One"?
			return mData >> Shift;
		}
		constexpr auto round() const -> value_type {
			return (*this + 0.5).partWhole();
		}
		constexpr auto real() const -> real_type {
			return static_cast<real_type>(mData) / One;
		}

		// misc functions -------------------------------@/
		constexpr auto lerp(const QFxi<S>& target, const QFxi<S>& alpha) const -> QFxi<S> {
			return *this + (target-*this) * alpha;
		}
		constexpr auto bezier(const QFxi<S>& pB, const QFxi<S>& pC, const QFxi<S>& alpha) const -> QFxi<S> {
			return lerp(pB,alpha).lerp(pB.lerp(pC,alpha),alpha);
		}
		constexpr auto sqrt_old() const -> QFxi<S> {
			// http://stackoverflow.com/questions/1100090/looking-for-an-efficient-integer-square-root-algorithm-for-arm-thumb2
			// https://github.com/melonDS-emu/melonDS/blob/906e9ebb27da8c6a715cd7abab4abfe8a8d29427/src/NDS.cpp#L1876
			auto res = std::sqrt( real() );
			return QFxi<S>(res);
		}
		constexpr auto sqrt() const -> QFxi<S> {
			// NOTE: we can either:
			// * shift left before, using the num of fractional bits
			// * shift right after, using HALF amount of fractional bits
			// the first choice is best, but COULD be prone to overflow.
			auto input = static_cast<int64_t>(raw()) << S;
			auto res = scl::math::sqrti(input);
			return QFxi<S>(res,0);
		}

		// operator overloads ---------------------------@/
		constexpr auto operator<<(const std::size_t amount) const -> QFxi<S> {
			return mData << amount;
		}
		constexpr auto operator>>(const std::size_t amount) const -> QFxi<S> {
			return mData >> amount;
		}
		constexpr auto operator+(const QFxi<S>& other) const -> QFxi<S> {
			return QFxi<S>(mData + other.raw(),0);
		}
		constexpr auto operator-(const QFxi<S>& other) const -> QFxi<S> {
			return QFxi<S>(mData - other.raw(),0);
		}
		constexpr auto operator-() const -> QFxi<S> {
			return QFxi<S>(-mData,0);
		}
		constexpr auto operator*(const QFxi<S>& opan) const -> QFxi<S> {
			int64_t val = ((int64_t)mData) * ((int64_t)opan.raw()) >> S;
			return QFxi<S>(val,0);
		}
		constexpr auto operator/(const QFxi<S>& opan) const -> QFxi<S> {
			int64_t val = (((int64_t)mData) << S) / ((int64_t)opan.raw());
			return QFxi<S>(val,0);
		}

		// assignment operators -------------------------@/
		constexpr auto operator+=(const QFxi<S>& other) -> QFxi<S>& {
			*this = *this + other;
			return *this;
		}
		constexpr auto operator-=(const QFxi<S>& other) -> QFxi<S>& {
			*this = *this - other;
			return *this;
		}
		constexpr auto operator*=(const QFxi<S>& other) -> QFxi<S>& {
			*this = *this * other;
			return *this;
		}
		constexpr auto operator/=(const QFxi<S>& other) -> QFxi<S>& {
			*this = *this / other;
			return *this;
		}

		// static fns -----------------------------------@/
		static constexpr auto cosb(int angle, const QFxi<S> len = 1) -> QFxi<S> {
			angle &= 0xFFFF;
			auto angle_f = static_cast<real_type>(angle) * (std::numbers::pi_v<real_type> / 32768.0);
			return QFxi<S>(std::cosf(angle_f)) * len;
		}
		static constexpr auto sinb(int angle, const QFxi<S> len = 1) -> QFxi<S> {
			return cosb(angle + 0x4000,len);
		}

		// misc fns -------------------------------------@/
		auto to_str() const -> std::string {
			if constexpr (S == 8) {
				return std::format("{0:d}.{1:02X}h",partWhole(),partFrac());
			} else if constexpr (S >= 9 || S <= 12) {
				return std::format("{0:d}.{1:03X}h",partWhole(),partFrac());
			} else if constexpr (S >= 13 || S <= 16) {
				return std::format("{0:d}.{1:04X}h",partWhole(),partFrac());
			} else {
				return std::format("{0:d}.{1:06X}h",partWhole(),partFrac());
			}
			/*
			*/
		}
};

template<std::size_t S> constexpr auto operator+(int a, const QFxi<S>& b) { return QFxi<S>(a) + b; }
template<std::size_t S> constexpr auto operator+(double a, const QFxi<S>& b) { return QFxi<S>(a) + b; }
template<std::size_t S> constexpr auto operator-(int a, const QFxi<S>& b) { return QFxi<S>(a) - b; }
template<std::size_t S> constexpr auto operator-(double a, const QFxi<S>& b) { return QFxi<S>(a) - b; }
template<std::size_t S> constexpr auto operator*(int a, const QFxi<S>& b) { return QFxi<S>(a) * b; }
template<std::size_t S> constexpr auto operator*(double a, const QFxi<S>& b) { return QFxi<S>(a) * b; }
template<std::size_t S> constexpr auto operator/(int a, const QFxi<S>& b) { return QFxi<S>(a) / b; }
template<std::size_t S> constexpr auto operator/(double a, const QFxi<S>& b) { return QFxi<S>(a) / b; }

typedef QFxi<> Fxi;

// --------------------------------------------------------------------------@/
// vector class
// --------------------------------------------------------------------------@/
template<typename T, std::size_t N = 4> class Vec {
	public:
		std::array<T,N> mData;
		constexpr Vec() : mData{} {}
		template<typename ...T0> constexpr Vec(T0... v) : mData{v...} {}

		// access methods -------------------------------@/
		constexpr auto operator[](const std::size_t idx) -> T& {
			return mData[idx];
		}
		constexpr auto operator[](const std::size_t idx) const -> const T& {
			return mData[idx];
		}
		constexpr auto at(const std::size_t idx) -> T& {
			return mData.at(idx);
		}
		constexpr auto at(const std::size_t idx) const -> const T& {
			return mData.at(idx);
		}

		// misc fns -------------------------------------@/
		constexpr auto size() const -> std::size_t {
			return N;
		}
		constexpr auto sum() const -> T {
			T res = 0;
			for(std::size_t i=0; i<N; i++) {
				res += mData[i];
			}
			return res;
		}
		constexpr auto lerp(const Vec<T,N>& other,const T alpha) const -> Vec<T,N> {
			Vec<T,N> res;
			for(std::size_t i=0; i<N; i++) {
				res[i] = mData[i] + (other[i]-mData[i]) * alpha;
			}
			return res;
		}
		constexpr auto dot(const Vec<T,N>& other) const -> T {
			T res = 0;
			for(std::size_t i=0; i<N; i++) {
				res += mData[i] * other[i];
			}
			return res;
		}
		constexpr auto magnitude() const -> T {
			T res = 0;
			for(std::size_t i=0; i<N; i++) {
				res += mData[i] * mData[i];
			}

			// specialization for fixed-point types ------@/
			if constexpr (std::is_same<T,Fxi>::value) {
				return res.sqrt();
			} else {
				return std::sqrt(res);
			}
			return res;
		}
		constexpr auto distance(const Vec<T,N>& other) const ->  T {
			return (other - *this).magnitude();
		}
		constexpr auto unit() const -> Vec<T,N> {
			return *this / magnitude();
		}

		// arith operators ------------------------------@/
		constexpr auto operator+(const Vec<T,N>& other) const -> Vec<T,N> {
			Vec<T,N> res;
			for(std::size_t i=0; i<N; i++) {
				res[i] = mData[i] + other[i];
			}
			return res;
		}
		constexpr auto operator-() const -> Vec<T,N> {
			Vec<T,N> res;
			for(std::size_t i=0; i<N; i++) {
				res[i] = -mData[i];
			}
			return res;
		}
		constexpr auto operator-(const Vec<T,N>& other) const -> Vec<T,N> {
			Vec<T,N> res;
			for(std::size_t i=0; i<N; i++) {
				res[i] = mData[i] - other[i];
			}
			return res;
		}
		constexpr auto operator/(const Vec<T,N>& other) const -> Vec<T,N> {
			Vec<T,N> res;
			for(std::size_t i=0; i<N; i++) {
				res[i] = mData[i] / other[i];
			}
			return res;
		}
		constexpr auto operator/(const T opan) const -> Vec<T,N> {
			Vec<T,N> res;
			for(std::size_t i=0; i<N; i++) {
				res[i] = mData[i] / opan;
			}
			return res;
		}
		constexpr auto operator*(const Vec<T,N>& other) const -> Vec<T,N> {
			Vec<T,N> res;
			for(std::size_t i=0; i<N; i++) {
				res[i] = mData[i] * other[i];
			}
			return res;
		}
		constexpr auto operator*(const T opan) const -> Vec<T,N> {
			Vec<T,N> res;
			for(std::size_t i=0; i<N; i++) {
				res[i] = mData[i] * opan;
			}
			return res;
		}
		constexpr auto operator*(const float other) const -> Vec<T,N> {
			return *this * other;
		}

		// assignment operators -------------------------@/
		constexpr auto operator=(const Vec<T,N>& other) -> Vec<T,N>& = default;
		
		auto to_str() const -> std::string {
			std::string str = "[ ";
			for(std::size_t i=0; i<N; i++) {
				if constexpr(std::is_same<T,Fxi>::value) {
					str += std::to_string(mData[i].real());
				} else {
					str += std::to_string(mData[i]);
				}

				// comma, but not at end ----------------@/
				if(i != N-1) {
					str += ", ";
				}
			}
			str += " ]";
			return str;
		}
};

typedef Vec<Fxi,2> Vec2fxi;
typedef Vec<float,2> Vec2f;
typedef Vec<int32_t,2> Vec2i;

typedef Vec<Fxi,3> Vec3fxi;
typedef Vec<float,3> Vec3f;
typedef Vec<int32_t,3> Vec3i;

typedef Vec<Fxi,4> Vec4fxi;
typedef Vec<float> Vec4f;
typedef Vec<int32_t> Vec4i;

typedef Vec4f Vecf;
typedef Vec4i Veci;
typedef Vec4fxi Vecfxi;

}; // namespace math
}; // namespace scl

