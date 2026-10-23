#pragma once
#include "Resource.h"

class Image : public Resource
{
public:
	using Resource::Resource;

	std::string GetTypeName() const override;
	void Unload() override;

	int GetWidth() const { return width; }
	int GetHeight() const { return height; }
	int GetChannels() const { return channels; }
	bool Is16Bit() const { return is16Bit; }
	bool HasAlpha() const { return channels == 2 || channels == 4; }

protected:
	void OnReload() override;
	void DrawPreview() override;
	void DrawProperties() override;

private:
	int width = 0;
	int height = 0;
	int channels = 0;
	bool is16Bit = false;
	bool valid = false;   
};