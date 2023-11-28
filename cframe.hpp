#pragma once

#include <matrix3x3.hpp>
#include <matrix4x4.hpp>
#include <quaternion.hpp>
#include <vector3.hpp>
#include <vector4.hpp>

struct RowMajorCFrameData {
	Vector4 data[3];
};

class CFrame {
	public:
		static CFrame from_axis_angle(const Vector3& axis, float angle);
		static CFrame from_euler_angles_xyz(float rx, float ry, float rz);
		static CFrame look_at(const Vector3& eye, const Vector3& center, const Vector3& up = Vector3(0, 1, 0));

		CFrame() = default;

		explicit CFrame(float diagonal)
				: m_columns{{diagonal, 0.f, 0.f}, {0.f, diagonal, 0.f}, {0.f, 0.f, diagonal}, {}}
			{}

		CFrame(float x, float y, float z)
				: m_columns{{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {x, y, z}} {}

		template <typename Vec3>
		explicit CFrame(Vec3 pos)
				: m_columns{{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, std::forward<Vec3>(pos)} {}

		template <typename Vec3>
		CFrame(Vec3 pos, const Quaternion& rot)
				: m_columns{{}, {}, {}, std::forward<Vec3>(pos)} {
			get_rotation_matrix() = glm::mat3_cast(rot);
		}

		template <typename T, typename U, typename V, typename W>
		CFrame(T pos, U vX, V vY, W vZ)
				: m_columns{std::forward<U>(vX), std::forward<V>(vY), std::forward<W>(vZ),
						std::forward<T>(pos)} {}

		CFrame(float x, float y, float z, float r00, float r01, float r02, float r10, float r11,
					float r12, float r20, float r21, float r22)
				: m_columns{{r00, r01, r02}, {r10, r11, r12}, {r20, r21, r22}, {x, y, z}} {}

		explicit CFrame(const Matrix4x4& m)
				: m_columns{Vector3(m[0]), Vector3(m[1]), Vector3(m[2]), Vector3(m[3])} {}

		CFrame& operator+=(const Vector3& v) {
			get_position() += v;
			return *this;
		}

		CFrame& operator-=(const Vector3& v) {
			get_position() -= v;
			return *this;
		}

		CFrame& operator*=(const CFrame& other) {
			get_position() += get_rotation_matrix() * other.get_position();
			get_rotation_matrix() *= other.get_rotation_matrix();

			return *this;
		}

		Vector3 operator*(const Vector3& v) const {
			return get_rotation_matrix() * v + get_position();
		}

		CFrame& scale_self_by(const Vector3& v) {
			get_rotation_matrix()[0] *= v.x;
			get_rotation_matrix()[1] *= v.y;
			get_rotation_matrix()[2] *= v.z;

			return *this;
		}

		CFrame scale_by(const Vector3& v) const {
			return CFrame(*this).scale_self_by(v);
		}

		CFrame& fast_inverse_self() {
			get_rotation_matrix() = glm::transpose(get_rotation_matrix());
			m_columns[3] = get_rotation_matrix() * -m_columns[3];

			return *this;
		}

		CFrame& inverse_self() {
			get_rotation_matrix() = glm::inverse(get_rotation_matrix());
			m_columns[3] = get_rotation_matrix() * -m_columns[3];

			return *this;
		}

		CFrame fast_inverse() const {
			return CFrame(*this).fast_inverse_self();
		}

		CFrame inverse() const {
			return CFrame(*this).inverse_self();
		}

		CFrame& lerp_self(const CFrame& goal, float alpha) {
			m_columns[0] = glm::normalize(glm::mix(m_columns[0], goal.m_columns[0], alpha));
			m_columns[1] = glm::normalize(glm::mix(m_columns[1], goal.m_columns[0], alpha));
			m_columns[2] = glm::normalize(glm::mix(m_columns[2], goal.m_columns[0], alpha));
			m_columns[3] = glm::mix(m_columns[3], goal.m_columns[0], alpha);
			return *this;
		}

		CFrame lerp(const CFrame& goal, float alpha) const {
			return CFrame(*this).lerp_self(goal, alpha);
		}

		 Vector3& operator[](size_t index) {
			return m_columns[index];
		}

		 const Vector3& operator[](size_t index) const {
			return m_columns[index];
		}

		Matrix3x3& get_rotation_matrix() {
			return *reinterpret_cast<Matrix3x3*>(this);
		}

		const Matrix3x3& get_rotation_matrix() const {
			return *reinterpret_cast<const Matrix3x3*>(this);
		}

		 Vector3& get_position() {
			return m_columns[3];
		}

		const Vector3& get_position() const {
			return m_columns[3];
		}

		const Vector3& right_vector() const {
			return m_columns[0];
		}

		const Vector3& up_vector() const {
			return m_columns[1];
		}

		Vector3 look_vector() const {
			return -m_columns[2];
		}

		const Vector3& x_vector() const {
			return m_columns[0];
		}

		const Vector3& y_vector() const {
			return m_columns[1];
		}

		const Vector3& z_vector() const {
			return m_columns[2];
		}

		Vector3 get_scale() const {
			return Vector3(length(m_columns[0]), length(m_columns[1]), length(m_columns[2]));
		}

		 Matrix4x4 to_matrix4x4() const {
			return Matrix4x4(Vector4(m_columns[0], 0.f), Vector4(m_columns[1], 0.f), Vector4(m_columns[2], 0.f),
					Vector4(m_columns[3], 1.f));
		}

		Quaternion to_quaternion() const {
			float trace = m_columns[0][0] + m_columns[1][1] + m_columns[2][2];

			if (trace > 0.f) {
				float s = 0.5f / sqrtf(trace + 1.f);
				return Quaternion(0.25f / s, (m_columns[2][1] - m_columns[1][2]) * s,
						(m_columns[0][2] - m_columns[2][0]) * s,
						(m_columns[1][0] - m_columns[0][1]) * s);
			}
			else {
				if (m_columns[0][0] > m_columns[1][1] && m_columns[0][0] > m_columns[2][2]) {
					float s = 2.f * sqrtf(1.f + m_columns[0][0] - m_columns[1][1] - m_columns[2][2]);
					return Quaternion((m_columns[2][1] - m_columns[1][2]) / s, 0.25f * s,
							(m_columns[0][1] + m_columns[1][0]) / s,
							(m_columns[0][2] + m_columns[2][0]) / s);
				}
				else if (m_columns[1][1] > m_columns[2][2]) {
					float s = 2.f * sqrtf(1.f + m_columns[1][1] - m_columns[0][0] - m_columns[2][2]);
					return Quaternion((m_columns[0][2] - m_columns[2][0]) / s,
							(m_columns[0][1] + m_columns[1][0]) / s, 0.25f * s,
							(m_columns[1][2] + m_columns[2][1]) / s);
				}
				else {
					float s = 2.f * sqrtf(1.f + m_columns[2][2] - m_columns[0][0] - m_columns[1][1]);
					return Quaternion((m_columns[1][0] - m_columns[0][1]) / s,
							(m_columns[1][0] - m_columns[0][1]) / s,
							(m_columns[0][2] + m_columns[2][0]) / s, 0.25f * s);
				}
			}
		}

		RowMajorCFrameData to_row_major() const {
			return {
				Vector4{m_columns[0][0], m_columns[1][0], m_columns[2][0], m_columns[3][0]},
				Vector4{m_columns[0][1], m_columns[1][1], m_columns[2][1], m_columns[3][1]},
				Vector4{m_columns[0][2], m_columns[1][2], m_columns[2][2], m_columns[3][2]},
			};
		}

		void to_euler_angles_xyz(float& x, float& y, float& z) const {
			float t1 = atan2(m_columns[2][1], m_columns[2][2]);
			float c2 = sqrt(m_columns[0][0] * m_columns[0][0] + m_columns[1][0] * m_columns[1][0]);
			float t2 = atan2(-m_columns[2][0], c2);
			float s1 = sin(t1);
			float c1 = cos(t1);
			float t3 = atan2(s1 * m_columns[0][2] - c1 * m_columns[0][1],
					c1 * m_columns[1][1] - s1 * m_columns[1][2]);

			x = -t1;
			y = -t2;
			z = -t3;
		}
	private:
		Vector3 m_columns[4];
};

inline CFrame operator*(const CFrame& a, const CFrame& b) {
	auto result = a;
	result *= b;
	return result;
}

inline CFrame operator+(const CFrame& a, const Vector3& b) {
	auto result = a;
	result += b;
	return result;
}

inline CFrame operator-(const CFrame& a, const Vector3& b) {
	auto result = a;
	result -= b;
	return result;
}

inline CFrame CFrame::from_axis_angle(const Vector3& axis, float angle) {
	float c = cos(angle);
	float s = sin(angle);
	float t = 1.f - c;

	float r00 = c + axis.x * axis.x * t;
	float r11 = c + axis.y * axis.y * t;
	float r22 = c + axis.z * axis.z * t;

	float tmp1 = axis.x * axis.y * t;
	float tmp2 = axis.z * s;

	float r01 = tmp1 + tmp2;
	float r10 = tmp1 - tmp2;

	float tmp3 = axis.x * axis.z * t;
	float tmp4 = axis.y * s;

	float r02 = tmp3 - tmp4;
	float r20 = tmp3 + tmp4;

	float tmp5 = axis.y * axis.z * t;
	float tmp6 = axis.x * s;
	
	float r12 = tmp5 + tmp6;
	float r21 = tmp5 - tmp6;

	return CFrame(0.f, 0.f, 0.f, r00, r01, r02, r10, r11, r12, r20, r21, r22);
}

inline CFrame CFrame::from_euler_angles_xyz(float rX, float rY, float rZ) {
	return from_axis_angle(Vector3(0, 0, 1), rZ) * from_axis_angle(Vector3(0, 1, 0), rY)
			* from_axis_angle(Vector3(1, 0, 0), rX);
}

inline CFrame CFrame::look_at(const Vector3& eye, const Vector3& center, const Vector3& up) {
	auto fwd = normalize(eye - center);
	auto right = normalize(cross(up, fwd));
	auto actualUp = cross(fwd, right);

	return CFrame(eye, std::move(right), std::move(actualUp), std::move(fwd));
}

