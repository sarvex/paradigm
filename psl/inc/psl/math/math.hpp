#pragma once
#include "vec.h"
#include "matrix.h"
#include "quaternion.h"
#include "utility.h"

#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif

constexpr std::size_t operator"" _sz(unsigned long long n) { return n; }

namespace psl
{

	template <typename precision_t>
	constexpr psl::tvec<precision_t, 3> operator*(const psl::tquat<precision_t>& quat,
												  const psl::tvec<precision_t, 3>& vec) noexcept
	{
		const tvec<precision_t, 3> qVec{quat[0], quat[1], quat[2]};
		const tvec<precision_t, 3> uv(psl::math::cross(qVec, vec));
		const tvec<precision_t, 3> uuv(psl::math::cross(qVec, uv));

		return vec + ((uv * quat[3]) + uuv) * precision_t{2};
	}
} // namespace psl

namespace psl::math
{
	template <typename precision_t>
	constexpr static precision_t sin(precision_t value) noexcept
	{
		return std::sin(value);
	}
	template <typename precision_t>
	constexpr static precision_t cos(precision_t value) noexcept
	{
		return std::cos(value);
	}
	template <typename precision_t>
	constexpr static precision_t tan(precision_t value) noexcept
	{
		return std::tan(value);
	}
	template <typename precision_t>
	constexpr static precision_t acos(precision_t value) noexcept
	{
		return std::acos(value);
	}
	template <typename precision_t>
	constexpr static precision_t asin(precision_t value) noexcept
	{
		return std::asin(value);
	}
	template <typename precision_t>
	constexpr static precision_t atan(precision_t value) noexcept
	{
		return std::atan(value);
	}

	template <typename precision_t>
	constexpr static precision_t sqrt(precision_t value) noexcept
	{
		return std::sqrt(value);
	}

	template <typename precision_t>
	constexpr static tquat<precision_t> angle_axis(const precision_t& angle,
												   const psl::tvec<precision_t, 3>& vec) noexcept
	{
		const precision_t s = sin(angle * precision_t{0.5});
		return tquat<precision_t>{vec[0] * s, vec[1] * s, vec[2] * s, cos(angle * precision_t{0.5})};
	}

	template <typename precision_t>
	constexpr static psl::tvec<precision_t, 3> rotate(const psl::tquat<precision_t>& quat,
													  const psl::tvec<precision_t, 3>& vec) noexcept
	{
		return quat * vec;
	}

	template <typename precision_t>
	constexpr inline precision_t fade(precision_t t) noexcept
	{
		return t * t * t * (t * (t * precision_t{6} - precision_t{15}) + precision_t{10});
	}

	template <typename hash_t, typename precision_t>
	constexpr inline precision_t grad(hash_t hash, precision_t x, precision_t y, precision_t z)
	{
		hash_t h = hash & 15;
		// Convert lower 4 bits of hash into 12 gradient directions
		precision_t u = h < 8 ? x : y, v = h < 4 ? y : h == 12 || h == 14 ? x : z;
		return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
	}

	template <typename hash_t, typename precision_t>
	constexpr inline precision_t grad(hash_t hash, precision_t x, precision_t y)
	{
		hash_t h = hash & 15;
		// Convert lower 4 bits of hash into 12 gradient directions
		precision_t u = h < 8 ? x : y, v = h < 4 ? y : h == 12 || h == 14 ? x : 1.0f;
		return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
	}

	template <typename precision_t>
	constexpr inline precision_t lerp(precision_t t, precision_t a, precision_t b) noexcept
	{
		return a + t * (b - a);
	}
	template <typename precision_t>
	constexpr inline psl::tvec<precision_t, 1> lerp(precision_t t, psl::tvec<precision_t, 1> a,
													psl::tvec<precision_t, 1> b) noexcept
	{
		return a + t * (b - a);
	}

	template <typename precision_t>
	constexpr inline psl::tvec<precision_t, 2> lerp(precision_t t, psl::tvec<precision_t, 2> a,
													psl::tvec<precision_t, 2> b) noexcept
	{
		return {lerp(t, a[0], b[0]), lerp(t, a[1], b[1])};
	}

	template <typename precision_t>
	constexpr inline psl::tvec<precision_t, 3> lerp(precision_t t, psl::tvec<precision_t, 3> a,
													psl::tvec<precision_t, 3> b) noexcept
	{
		return {lerp(t, a[0], b[0]), lerp(t, a[1], b[1]), lerp(t, a[2], b[2])};
	}

	template <typename precision_t>
	constexpr inline psl::tvec<precision_t, 4> lerp(precision_t t, psl::tvec<precision_t, 4> a,
													psl::tvec<precision_t, 4> b) noexcept
	{
		return {lerp(t, a[0], b[0]), lerp(t, a[1], b[1]), lerp(t, a[2], b[2]), lerp(t, a[3], b[3])};
	}
	template <typename precision_t>
	constexpr static precision_t saturate(precision_t value) noexcept
	{
		return std::clamp(value, precision_t{0}, precision_t{1});
	}

	template <typename precision_t>
	constexpr static precision_t clamp(precision_t value, precision_t min = 0, precision_t max = 1) noexcept
	{
		return std::clamp(value, min, max);
	}

	template <typename precision_t, size_t dimensions>
	constexpr static precision_t distance(const tvec<precision_t, dimensions>& vec1,
										  const tvec<precision_t, dimensions>& vec2) noexcept
	{
		return psl::math::magnitude(vec1 - vec2);
	}


	template <typename precision_t>
	constexpr static precision_t floor(precision_t value) noexcept
	{
		return std::floor(value);
	}

	template <typename precision_t>
	constexpr static psl::tvec<precision_t, 1> floor(psl::tvec<precision_t, 1> value) noexcept
	{
		return std::floor(value);
	}

	template <typename precision_t>
	constexpr static psl::tvec<precision_t, 2> floor(psl::tvec<precision_t, 2> value) noexcept
	{
		return {floor(value[0]), floor(value[1])};
	}

	template <typename precision_t>
	constexpr static psl::tvec<precision_t, 3> floor(psl::tvec<precision_t, 3> value) noexcept
	{
		return {floor(value[0]), floor(value[1]), floor(value[2])};
	}

	template <typename precision_t>
	constexpr static psl::tvec<precision_t, 4> floor(psl::tvec<precision_t, 4> value) noexcept
	{
		return {floor(value[0]), floor(value[1]), floor(value[2]), floor(value[3])};
	}

	template <typename precision_t>
	constexpr static precision_t exp(precision_t value) noexcept
	{
		return std::exp(value);
	}

	template <typename precision_t>
	constexpr static precision_t fract(precision_t value) noexcept
	{
		return value - floor(value);
	}

	template <typename precision_t>
	constexpr static auto log(precision_t value) noexcept
	{
		return std::log(value);
	}

	template <typename precision_t, typename precision_N_t>
	constexpr static long double log_n(precision_N_t N, precision_t value) noexcept
	{
		static_assert(std::is_convertible<precision_N_t, long double>::value,
					  "requires to be convertible to 'long double'");
		static_assert(std::is_convertible<precision_t, long double>::value,
					  "requires to be convertible to 'long double'");
		return log((long double)value) / log((long double)N);
	}


	template <typename precision_t, typename precision_N_t>
	constexpr static precision_t next_pow_of(precision_N_t N, precision_t value) noexcept
	{
		// static_assert(std::is_unsigned<precision_t>::value, "requires unsigned type");
		static_assert(std::is_convertible<precision_t, long double>::value,
					  "requires to be convertible to 'long double'");

		return static_cast<precision_t>(std::pow<precision_t>(N, static_cast<precision_t>(std::ceil(log_n(N, value)))));
	}

	/// \brief round to nearest multiple of N
	/// \param[in] N the multiple to round to
	/// \param[in] value the value to round
	template <typename precision_t>
	constexpr static precision_t round_to(precision_t N, precision_t value) noexcept
	{
		constexpr auto remainder = value % N;
		constexpr auto extra	 = N - remainder;
		if(extra < remainder)
		{
			return value + extra;
		}
		return value - remainder;
	}


	/// \brief ceil to nearest multiple of N
	/// \param[in] N the multiple to ceil to
	/// \param[in] value the value to ceil
	template <typename precision_t>
	constexpr static precision_t ceil_to(precision_t N, precision_t value) noexcept
	{
		constexpr auto remainder = value % N;
		return value + (N - remainder);
	}

	/// \brief floor to nearest multiple of N
	/// \param[in] N the multiple to floor to
	/// \param[in] value the value to floor
	template <typename precision_t>
	constexpr static precision_t floor_to(precision_t N, precision_t value) noexcept
	{
		constexpr auto remainder = value % N;
		return value - remainder;
	}

	template <typename precision_t>
	constexpr static inline precision_t min(const precision_t& left, const precision_t& right) noexcept
	{
		return std::min(left, right);
	}

	template <typename precision_t, size_t N>
	constexpr static inline tvec<precision_t, N> min(const tvec<precision_t, N>& left,
													 const tvec<precision_t, N>& right) noexcept
	{
		tvec<precision_t, N> res;
		for(auto i = 0; i < N; ++i) res[i] = min<precision_t>(left[i], right[i]);
		return res;
	}

	template <typename precision_t>
	constexpr static inline precision_t max(const precision_t& left, const precision_t& right) noexcept
	{
		return std::max(left, right);
	}

	template <typename precision_t, size_t N>
	constexpr static inline tvec<precision_t, N> max(const tvec<precision_t, N>& left,
													 const tvec<precision_t, N>& right) noexcept
	{
		tvec<precision_t, N> res;
		for(auto i = 0; i < N; ++i) res[i] = max<precision_t>(left[i], right[i]);
		return res;
	}

	template <typename precision_t>
	constexpr static inline precision_t abs(const precision_t& value) noexcept
	{
		return std::abs(value);
	}

	template <typename precision_t, size_t N>
	constexpr static inline tvec<precision_t, N> abs(const tvec<precision_t, N>& value) noexcept
	{
		tvec<precision_t, N> res;
		for(auto i = 0; i < N; ++i) res[i] = abs<precision_t>(value[i]);
		return res;
	}

	template <typename precision_t>
	constexpr static inline precision_t difference(const precision_t& lhs, const precision_t& rhs) noexcept
	{
		return (lhs < rhs) ? abs<precision_t>(rhs - lhs) : abs<precision_t>(lhs - rhs);
	}

	template <typename precision_t, size_t N>
	constexpr static inline tvec<precision_t, N> difference(const tvec<precision_t, N>& lhs,
															const tvec<precision_t, N>& rhs) noexcept
	{
		tvec<precision_t, N> res;
		for(auto i = 0; i < N; ++i)
			res[i] = difference<precision_t>(lhs[i], rhs[i]);
		return res;
	}

} // namespace psl::math


/// conversions
namespace psl::math
{

	template <typename precision_t>
	constexpr static tquat<precision_t> from_euler(precision_t pitch, precision_t yaw, precision_t roll) noexcept
	{
		constexpr precision_t half{precision_t{1} / precision_t{2}};
		pitch		   = psl::math::radians(pitch) * half;
		yaw			   = psl::math::radians(yaw) * half;
		roll		   = psl::math::radians(roll) * half;
		precision_t cy = cos(roll);
		precision_t sy = sin(roll);
		precision_t cr = cos(yaw);
		precision_t sr = sin(yaw);
		precision_t cp = cos(pitch);
		precision_t sp = sin(pitch);

		return tquat<precision_t>{cy * cr * cp + sy * sr * sp, cy * cr * sp + sy * sr * cp, cy * sr * cp - sy * cr * sp,
								  sy * cr * cp - cy * sr * sp};
	}
	template <typename precision_t>
	constexpr static tquat<precision_t> from_euler(const psl::tvec<precision_t, 3>& vec) noexcept
	{
		return from_euler(vec[0], vec[1], vec[2]);
	}

	template <typename precision_t>
	constexpr static tvec<precision_t, 3> to_euler(const tquat<precision_t>& q) noexcept
	{
		tvec<precision_t, 3> res;
		precision_t sqw	 = q[3] * q[3];
		precision_t sqx	 = q[0] * q[0];
		precision_t sqy	 = q[1] * q[1];
		precision_t sqz	 = q[2] * q[2];
		precision_t unit = sqx + sqy + sqz + sqw; // if normalised is one, otherwise is correction factor
		precision_t test = q[0] * q[1] + q[2] * q[3];
		if(test > precision_t{100} / precision_t{201} * unit)
		{ // singularity at north pole
			res[0] = precision_t{2} * atan2(q[0], q[3]);
			res[1] = constants<precision_t>::PI / precision_t{2};
			res[2] = precision_t{0};
		}
		else if(test < -precision_t{100} / precision_t{201} * unit)
		{ // singularity at south pole
			res[0] = -precision_t{2} * atan2(q[0], q[3]);
			res[1] = -constants<precision_t>::PI / precision_t{2};
			res[2] = 0;
		}
		else
		{
			res[0] = atan2(precision_t{2} * q[1] * q[3] - 2 * q[0] * q[2], sqx - sqy - sqz + sqw);
			res[1] = asin(precision_t{2} * test / unit);
			res[2] = atan2(precision_t{2} * q[0] * q[3] - 2 * q[1] * q[2], -sqx + sqy - sqz + sqw);
		}
		res[0] = degrees(res[0]);
		res[1] = degrees(res[1]);
		res[2] = degrees(res[2]);
		return res;
	}


	template <typename precision_t>
	constexpr static psl::tmat<precision_t, 4, 4> perspective_projection(precision_t vertical_fov,
																		 precision_t aspect_ratio, precision_t near,
																		 precision_t far) noexcept
	{
		precision_t const thf = tan(vertical_fov / precision_t{2});
		psl::tmat<precision_t, 4, 4> res(precision_t{0});
		res[{0, 0}] = precision_t{1} / (aspect_ratio * thf);
		res[{1, 1}] = precision_t{1} / (thf);
		res[{2, 3}] = -precision_t{1};
		res[{2, 2}] = far / (near - far);
		res[{3, 2}] = -(far * near) / (far - near);
		return res;
	}

	template <typename precision_t>
	constexpr static psl::tmat<precision_t, 4, 4> look_at(const psl::tvec<precision_t, 3>& eye,
														  const psl::tvec<precision_t, 3>& center,
														  const psl::tvec<precision_t, 3>& up) noexcept
	{
		psl::tvec<precision_t, 3> const f(normalize(center - eye));
		psl::tvec<precision_t, 3> const s(normalize(cross(f, up)));
		psl::tvec<precision_t, 3> const u(cross(s, f));

		psl::tmat<precision_t, 4, 4> res{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
		res[{0, 0}] = s[0];
		res[{1, 0}] = s[1];
		res[{2, 0}] = s[2];
		res[{0, 1}] = u[0];
		res[{1, 1}] = u[1];
		res[{2, 1}] = u[2];
		res[{0, 2}] = -f[0];
		res[{1, 2}] = -f[1];
		res[{2, 2}] = -f[2];
		res[{3, 0}] = -dot(s, eye);
		res[{3, 1}] = -dot(u, eye);
		res[{3, 2}] = dot(f, eye);
		return res;
	}


	template <typename precision_t>
	constexpr static psl::tmat<precision_t, 4, 4> scale(const psl::tmat<precision_t, 4, 4>& mat,
														const psl::tvec<precision_t, 3>& vec) noexcept
	{
		psl::tmat<precision_t, 4, 4> res{};
		res.row(0, mat.row(0) * vec[0]);
		res.row(1, mat.row(1) * vec[1]);
		res.row(2, mat.row(2) * vec[2]);
		res.row(3, mat.row(3));
		return res;
	}

	template <typename precision_t>
	constexpr static psl::tmat<precision_t, 4, 4> scale(const psl::tvec<precision_t, 3>& vec) noexcept
	{
		const psl::tmat<precision_t, 4, 4> mat{1};
		psl::tmat<precision_t, 4, 4> res{};
		res.row(0, mat.row(0) * vec[0]);
		res.row(1, mat.row(1) * vec[1]);
		res.row(2, mat.row(2) * vec[2]);
		res.row(3, mat.row(3));
		return res;
	}

	template <typename precision_t>
	constexpr static psl::tmat<precision_t, 4, 4> translate(const psl::tmat<precision_t, 4, 4>& mat,
															const psl::tvec<precision_t, 3>& vec) noexcept
	{
		psl::tmat<precision_t, 4, 4> res{mat};
		res.row(3, mat.row(0) * vec[0] + mat.row(1) * vec[1] + mat.row(2) * vec[2] + mat.row(3));
		return res;
	}

	template <typename precision_t>
	constexpr static psl::tmat<precision_t, 4, 4> translate(const psl::tvec<precision_t, 3>& vec) noexcept
	{
		const psl::tmat<precision_t, 4, 4> mat{1};
		psl::tmat<precision_t, 4, 4> res{mat};
		res.row(3, mat.row(0) * vec[0] + mat.row(1) * vec[1] + mat.row(2) * vec[2] + mat.row(3));
		return res;
	}

	template <typename precision_t>
	constexpr static psl::tmat<precision_t, 4, 4>
	rotate(const psl::tmat<precision_t, 4, 4>& tmat, const psl::tvec<precision_t, 3>& tvec, precision_t angle) noexcept
	{
		constexpr precision_t a = angle;
		constexpr precision_t c = cos(a);
		constexpr precision_t s = sin(a);

		psl::tvec<precision_t, 3> axis(normalize(tvec));
		psl::tvec<precision_t, 3> temp((precision_t{1} - c) * axis);

		psl::tmat<precision_t, 4, 4> res_int{};
		res_int[{0, 0}] = c + temp[0] * axis[0];
		res_int[{0, 1}] = temp[0] * axis[1] + s * axis[2];
		res_int[{0, 2}] = temp[0] * axis[2] - s * axis[1];

		res_int[{1, 0}] = temp[1] * axis[0] - s * axis[2];
		res_int[{1, 1}] = c + temp[1] * axis[1];
		res_int[{1, 2}] = temp[1] * axis[2] + s * axis[0];

		res_int[{2, 0}] = temp[2] * axis[0] + s * axis[1];
		res_int[{2, 1}] = temp[2] * axis[1] - s * axis[0];
		res_int[{2, 2}] = c + temp[2] * axis[2];

		psl::tmat<precision_t, 4, 4> res{};
		res.row(0, tmat.row(0) * res_int[{0, 0}] + tmat.row(1) * res_int[{0, 1}] + tmat.row(2) * res_int[{0, 2}]);
		res.row(1, tmat.row(0) * res_int[{1, 0}] + tmat.row(1) * res_int[{1, 1}] + tmat.row(2) * res_int[{1, 2}]);
		res.row(2, tmat.row(0) * res_int[{2, 0}] + tmat.row(1) * res_int[{2, 1}] + tmat.row(2) * res_int[{2, 2}]);
		res.row(3, tmat.row(3));
		return res;
	}

	template <typename precision_t>
	constexpr static tmat<precision_t, 3, 3> to_matrix(const psl::tquat<precision_t>& value) noexcept
	{
		tmat<precision_t, 3, 3> res{1};
		precision_t qxx(value[0] * value[0]);
		precision_t qyy(value[1] * value[1]);
		precision_t qzz(value[2] * value[2]);
		precision_t qxz(value[0] * value[2]);
		precision_t qxy(value[0] * value[1]);
		precision_t qyz(value[1] * value[2]);
		precision_t qwx(value[3] * value[0]);
		precision_t qwy(value[3] * value[1]);
		precision_t qwz(value[3] * value[2]);

		res[{0, 0}] = precision_t{1} - precision_t{2} * (qyy + qzz);
		res[{0, 1}] = precision_t{2} * (qxy + qwz);
		res[{0, 2}] = precision_t{2} * (qxz - qwy);

		res[{1, 0}] = precision_t{2} * (qxy - qwz);
		res[{1, 1}] = precision_t{1} - precision_t{2} * (qxx + qzz);
		res[{1, 2}] = precision_t{2} * (qyz + qwx);

		res[{2, 0}] = precision_t{2} * (qxz + qwy);
		res[{2, 1}] = precision_t{2} * (qyz - qwx);
		res[{2, 2}] = precision_t{1} - precision_t{2} * (qxx + qyy);
		return res;
	};

	template <typename precision_t>
	static constexpr tquat<precision_t> to_quat(tmat<precision_t, 3, 3> const& mat) noexcept
	{
		precision_t fourXSquaredMinus1 = mat[{0, 0}] - mat[{1, 1}] - mat[{2, 2}];
		precision_t fourYSquaredMinus1 = mat[{1, 1}] - mat[{0, 0}] - mat[{2, 2}];
		precision_t fourZSquaredMinus1 = mat[{2, 2}] - mat[{0, 0}] - mat[{1, 1}];
		precision_t fourWSquaredMinus1 = mat[{0, 0}] + mat[{1, 1}] + mat[{2, 2}];

		int biggestIndex					 = 0;
		precision_t fourBiggestSquaredMinus1 = fourWSquaredMinus1;
		if(fourXSquaredMinus1 > fourBiggestSquaredMinus1)
		{
			fourBiggestSquaredMinus1 = fourXSquaredMinus1;
			biggestIndex			 = 1;
		}
		if(fourYSquaredMinus1 > fourBiggestSquaredMinus1)
		{
			fourBiggestSquaredMinus1 = fourYSquaredMinus1;
			biggestIndex			 = 2;
		}
		if(fourZSquaredMinus1 > fourBiggestSquaredMinus1)
		{
			fourBiggestSquaredMinus1 = fourZSquaredMinus1;
			biggestIndex			 = 3;
		}

		precision_t biggestVal =
			std::sqrt(fourBiggestSquaredMinus1 + static_cast<precision_t>(1)) * static_cast<precision_t>(0.5);
		precision_t mult = static_cast<precision_t>(0.25) / biggestVal;

		switch(biggestIndex)
		{
		case 0:
			return tquat<precision_t>((mat[{1, 2}] - mat[{2, 1}]) * mult, (mat[{2, 0}] - mat[{0, 2}]) * mult,
									  (mat[{0, 1}] - mat[{1, 0}]) * mult, biggestVal);
		case 1:
			return tquat<precision_t>(biggestVal, (mat[{0, 1}] + mat[{1, 0}]) * mult,
									  (mat[{2, 0}] + mat[{0, 2}]) * mult, (mat[{1, 2}] - mat[{2, 1}]) * mult);
		case 2:
			return tquat<precision_t>((mat[{0, 1}] + mat[{1, 0}]) * mult, biggestVal,
									  (mat[{1, 2}] + mat[{2, 1}]) * mult, (mat[{2, 0}] - mat[{0, 2}]) * mult);
		case 3:
			return tquat<precision_t>((mat[{2, 0}] + mat[{0, 2}]) * mult, (mat[{1, 2}] + mat[{2, 1}]) * mult,
									  biggestVal, (mat[{0, 1}] - mat[{1, 0}]) * mult);
		default
			: // Silence a -Wswitch-default warning in GCC. Should never actually get here. Assert is just for sanity.
			assert(false);
			return tquat<precision_t>(0, 0, 0, 1);
		}
	}

	template <typename precision_t>
	constexpr static tquat<precision_t> look_at_q(const tvec<precision_t, 3>& direction,
												  const tvec<precision_t, 3>& up) noexcept
	{
		psl::tmat<precision_t, 3, 3> mat;

		mat.row(2, -direction);
		mat.row(0, normalize(cross(up, mat.row(2))));
		mat.row(1, cross(mat.row(2), mat.row(0)));

		return to_quat(mat);
	}
	template <typename precision_t>
	constexpr static tquat<precision_t> look_at_q(const tvec<precision_t, 3>& origin,
												  const tvec<precision_t, 3>& target,
												  const tvec<precision_t, 3>& up) noexcept
	{
		return look_at_q(normalize(origin - target), up);
	}


	template <typename precision_t>
	constexpr static precision_t mix(const precision_t& x, const precision_t& y, precision_t a) noexcept
	{
		return lerp(a, x, y);
	}

	template <typename element1_t, typename element2_t, typename precision_t, typename return_t = element1_t, size_t N>
	constexpr static psl::tvec<return_t, N> mix(const std::array<element1_t, N>& x, const std::array<element2_t, N>& y,
												precision_t a) noexcept
	{
		return lerp(a, x, y);
	}

	template <typename element1_t, typename element2_t, typename precision_t, typename return_t = element1_t, size_t N>
	constexpr static psl::tvec<return_t, N> mix(const psl::tvec<element1_t, N>& x, const psl::tvec<element2_t, N>& y,
												precision_t a) noexcept
	{
		return lerp(a, x, y);
	}

} // namespace psl::math