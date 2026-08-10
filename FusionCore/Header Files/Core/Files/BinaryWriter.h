#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <type_traits>
#include <stdexcept>
#include <glm/glm.hpp>

class BinaryWriter
{
public:
	explicit BinaryWriter(std::ostream& out) : out(out) {}

	template <typename T>
	void Write(const T& value)
	{
		static_assert(std::is_trivially_copyable_v<T>, "BinaryWriter::Write requires a trivially copyable type");
		out.write(reinterpret_cast<const char*>(&value), sizeof(T));
	}

	void Write(const glm::vec3& v)
	{
		Write(v.x);
		Write(v.y);
		Write(v.z);
	}

	void Write(const glm::vec4& v)
	{
		Write(v.x);
		Write(v.y);
		Write(v.z);
		Write(v.w);
	}

	void Write(const glm::mat4& m)
	{
		for (int col = 0; col < 4; col++)
			for (int row = 0; row < 4; row++)
				Write(m[col][row]);
	}

	void WriteString(const std::string& s)
	{
		uint32_t len = static_cast<uint32_t>(s.size());
		Write(len);
		if (len > 0)
			out.write(s.data(), len);
	}

	template <typename T>
	void WriteArray(const std::vector<T>& vec)
	{
		static_assert(std::is_trivially_copyable_v<T>, "WriteArray requires a trivially copyable element type");
		uint32_t count = static_cast<uint32_t>(vec.size());
		Write(count);
		if (count > 0)
			out.write(reinterpret_cast<const char*>(vec.data()), count * sizeof(T));
	}

	void WriteVec3Array(const std::vector<glm::vec3>& vec)
	{
		uint32_t count = static_cast<uint32_t>(vec.size());
		Write(count);
		for (uint32_t i = 0; i < count; i++)
			Write(vec[i]);
	}

	void WriteMat4Array(const std::vector<glm::mat4>& vec)
	{
		uint32_t count = static_cast<uint32_t>(vec.size());
		Write(count);
		for (uint32_t i = 0; i < count; i++)
			Write(vec[i]);
	}

	void WriteBytes(const void* data, size_t size)
	{
		out.write(reinterpret_cast<const char*>(data), size);
	}

	bool Good() const { return out.good(); }

private:
	std::ostream& out;
};