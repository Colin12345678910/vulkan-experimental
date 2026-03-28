#include <cstdint>
#include <string>
#include <unordered_map>
#include <imgui.h>

#pragma once

constexpr static int MAX_CVARS = 1024;


#define BY_NAME(str) std::hash<std::string>{}(str)


enum class CVarFlags : uint32_t
{
	None = 0,
	ReadOnly = 1 << 1
};
enum class CVarType {
	INT,
	FLOAT,
	STRING,
	BOOL
};
class CVarParameter
{
public:
	friend class CVarImpl;

	int32_t arrayIndex;

	CVarType type;
	CVarFlags flags;
	std::string name;
	std::string description;
};
class CVar
{
public:
	virtual CVarParameter* GetCVar(size_t hash) = 0;
	virtual CVarParameter* CreateFloatCVar(const char* name, const char* description, float defaultValue, float maxValue, CVarFlags flags = CVarFlags::None) = 0;
	virtual CVarParameter* CreateIntCVar(const char* name, const char* description, int32_t defaultValue, CVarFlags flags = CVarFlags::None) = 0;
	virtual CVarParameter* CreateStringCVar(const char* name, const char* description, const char* defaultValue, CVarFlags flags = CVarFlags::None) = 0;
	virtual CVarParameter* CreateBoolCVar(const char* name, const char* description, bool defaultValue, CVarFlags flags = CVarFlags::None) = 0;
	
	virtual float* GetFloatCVar(size_t hash) = 0;
	virtual void SetFloatCVar(size_t hash, float value) = 0;
	virtual int* GetIntCVar(size_t hash) = 0;
	virtual void SetIntCVar(size_t hash, int32_t value) = 0;
	virtual std::string* GetStringCVar(size_t hash) = 0;
	virtual void SetStringCVar(size_t hash, const std::string& value) = 0;
	virtual bool* GetBoolCVar(size_t hash) = 0;
	virtual void SetBoolCVar(size_t hash, bool value) = 0;

	virtual void ImGuiDisplayCVars() = 0;//
	static CVar* Get();
};
template <typename T>
struct AutoCVar
{
protected:
	int index;
	using CVarType = T;
};

struct AutoFloatCVar : public AutoCVar<float>
{
	AutoFloatCVar(const char* name, const char* description, float defaultValue, float maxValue = FLT_MAX, CVarFlags flags = CVarFlags::None);

	float Get();
	void Set(float val);
};

struct AutoIntCVar : public AutoCVar<int32_t>
{
	AutoIntCVar(const char* name, const char* description, int32_t defaultValue, CVarFlags flags = CVarFlags::None);
	int32_t Get();
	void Set(int32_t val);
};
struct AutoStringCVar : public AutoCVar<std::string>
{
	AutoStringCVar(const char* name, const char* description, const char* defaultValue, CVarFlags flags = CVarFlags::None);
	std::string Get();
	void Set(const std::string& val);
};
struct AutoBoolCVar : public AutoCVar<bool>
{
	AutoBoolCVar(const char* name, const char* description, bool defaultValue, CVarFlags flags = CVarFlags::None);
	bool Get();
	void Set(bool val);
};