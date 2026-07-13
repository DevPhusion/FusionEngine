#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <type_traits>
#include <stdexcept>
#include <glm/glm.hpp>

class BinaryReader
{
public:
	explicit BinaryReader(std::ifstream& in) : in(in) {}

	template <typename T>
	T Read()
	{
		static_assert(std::is_trivially_copyable_v<T>, "BinaryReader::Read requires a trivially copyable type");
		T value{};
		in.read(reinterpret_cast<char*>(&value), sizeof(T));
		if (!in.good())
			throw std::runtime_error("BinaryReader: unexpected end of file or read failure");
		return value;
	}

	template <>
	glm::vec3 Read<glm::vec3>()
	{
		glm::vec3 v;
		v.x = Read<float>();
		v.y = Read<float>();
		v.z = Read<float>();
		return v;
	}

	template <>
	glm::vec4 Read<glm::vec4>()
	{
		glm::vec4 v;
		v.x = Read<float>();
		v.y = Read<float>();
		v.z = Read<float>();
		v.w = Read<float>();
		return v;
	}

	template <>
	glm::mat4 Read<glm::mat4>()
	{
		glm::mat4 m;
		for (int col = 0; col < 4; col++)
			for (int row = 0; row < 4; row++)
				m[col][row] = Read<float>();
		return m;
	}

	std::string ReadString()
	{
		uint32_t len = Read<uint32_t>();
		if (len == 0) return std::string();

		constexpr uint32_t kMaxStringLen = 64 * 1024 * 1024;
		if (len > kMaxStringLen)
			throw std::runtime_error("BinaryReader: string length exceeds sanity limit");

		std::string s(len, '\0');
		in.read(s.data(), len);
		if (!in.good())
			throw std::runtime_error("BinaryReader: unexpected end of file while reading string");
		return s;
	}

	template <typename T>
	std::vector<T> ReadArray()
	{
		static_assert(std::is_trivially_copyable_v<T>, "ReadArray requires a trivially copyable element type");
		uint32_t count = Read<uint32_t>();

		constexpr uint32_t kMaxElementCount = 16 * 1024 * 1024;
		if (count > kMaxElementCount)
			throw std::runtime_error("BinaryReader: array element count exceeds sanity limit");

		std::vector<T> vec(count);
		if (count > 0)
		{
			in.read(reinterpret_cast<char*>(vec.data()), count * sizeof(T));
			if (!in.good())
				throw std::runtime_error("BinaryReader: unexpected end of file while reading array");
		}
		return vec;
	}

	std::vector<glm::vec3> ReadVec3Array()
	{
		uint32_t count = Read<uint32_t>();

		constexpr uint32_t kMaxElementCount = 16 * 1024 * 1024;
		if (count > kMaxElementCount)
			throw std::runtime_error("BinaryReader: vec3 array element count exceeds sanity limit");

		std::vector<glm::vec3> vec;
		vec.reserve(count);
		for (uint32_t i = 0; i < count; i++)
			vec.push_back(Read<glm::vec3>());
		return vec;
	}

	std::vector<glm::mat4> ReadMat4Array()
	{
		uint32_t count = Read<uint32_t>();

		constexpr uint32_t kMaxElementCount = 4 * 1024 * 1024;
		if (count > kMaxElementCount)
			throw std::runtime_error("BinaryReader: mat4 array element count exceeds sanity limit");

		std::vector<glm::mat4> vec;
		vec.reserve(count);
		for (uint32_t i = 0; i < count; i++)
			vec.push_back(Read<glm::mat4>());
		return vec;
	}

	void ReadBytes(void* dest, size_t size)
	{
		in.read(reinterpret_cast<char*>(dest), size);
		if (!in.good())
			throw std::runtime_error("BinaryReader: unexpected end of file while reading raw bytes");
	}

	void Skip(size_t bytes)
	{
		in.seekg(bytes, std::ios::cur);
	}

	bool Good() const { return in.good(); }
	bool Eof() const { return in.eof(); }

private:
	std::ifstream& in;
};