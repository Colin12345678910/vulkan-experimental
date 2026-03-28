#include "CVars.h"
/*
Console Vars - CVars are a way to expose variables to our game at runtime and into our IMGUI debug menu.
They can be changed at runtime and are a great way to tweak values without having to recompile.
*/

template<typename T>
struct CVarStorage
{
	T initial;
	T max;
	T current;
	CVarParameter* parameters;
};

/// <summary>
/// A specially designed data structure to handle storing all CVars of a specific type.
/// </summary>
/// <typeparam name="T">What type the Array should hold</typeparam>
template<typename T>
struct CVarArray
{
	CVarStorage<T>* cvars;
	int32_t lastCvarIndex{ 0 };

	CVarArray(size_t size)
	{
		cvars = new CVarStorage<T>[size];
	}
	~CVarArray()
	{
		delete[] cvars;
	}

	T GetCurrent(int32_t index)
	{
		return cvars[index].current;
	};

	void SetCurrent(int32_t index, T value)
	{
		cvars[index].current = value;
	};
	int Add(const T& val, CVarParameter* param)
	{
		int index = lastCvarIndex++;
		cvars[index].initial = val;
		cvars[index].max = val;
		cvars[index].current = val;
		cvars[index].parameters = param;

		param->arrayIndex = index;
		return index;
	};
	int Add(const T& initialVal, const T& currentVal, CVarParameter* param)
	{
		int index = lastCvarIndex++;
		cvars[index].current = currentVal;
		cvars[index].max = initialVal;
		cvars[index].initial = initialVal;
		cvars[index].parameters = param;

		param->arrayIndex = index;
		return index;
	}
	int Add(const T& initialVal, const T& currentVal, const T& maxVal, CVarParameter* param)
	{
		int index = lastCvarIndex++;
		cvars[index].current = currentVal;
		cvars[index].initial = initialVal;
		cvars[index].max = maxVal;
		cvars[index].parameters = param;

		param->arrayIndex = index;
		return index;
	}
};

class CVarImpl : public CVar
{
public:
	// Functions for Creating CVars.
	CVarParameter* GetCVar(size_t hash) override final;
	CVarParameter* CreateFloatCVar(const char* name, const char* description, float defaultValue, float maxValue, CVarFlags flags = CVarFlags::None) override final;
	CVarParameter* CreateIntCVar(const char* name, const char* description, int32_t defaultValue, CVarFlags flags = CVarFlags::None) override final;
	CVarParameter* CreateStringCVar(const char* name, const char* description, const char* defaultValue, CVarFlags flags = CVarFlags::None) override final;
	CVarParameter* CreateBoolCVar(const char* name, const char* description, bool defaultValue, CVarFlags flags = CVarFlags::None) override final;

	float* GetFloatCVar(size_t hash) override final;
	void SetFloatCVar(size_t hash, float value) override final;
	int* GetIntCVar(size_t hash) override final;
	void SetIntCVar(size_t hash, int32_t value) override final;
	std::string* GetStringCVar(size_t hash) override final;
	void SetStringCVar(size_t hash, const std::string& value) override final;
	bool* GetBoolCVar(size_t hash) override final;
	void SetBoolCVar(size_t hash, bool value) override final;
	void ImGuiDisplayCVars() override final;

	CVarArray<int32_t> intCVars{ MAX_CVARS };
	CVarArray<float> floatCVars{ MAX_CVARS };
	CVarArray<std::string> stringCVars{ MAX_CVARS };
	CVarArray<bool> boolCVars{ MAX_CVARS };
	
	//Special handling for each type of CVar
	template<typename T>
	CVarArray<T>* GetCVarArray();

	template<typename T>
	T* GetCVarCurrent(uint32_t hash)
	{
		CVarParameter* param = GetCVar(hash);
		if (!param)
		{
			return nullptr; // CVar not found
		}
		auto cvar = GetCVarArray<T>()->GetCurrent(param->arrayIndex);
		return &cvar;
	}

	template<typename T>
	void SetCVarCurrent(uint32_t hash, T value)
	{
		CVarParameter* param = GetCVar(hash);
		if (!param)
		{
			return; // CVar not found
		}
		GetCVarArray<T>()->SetCurrent(param->arrayIndex, value);
	}
private:
	CVarParameter* InitCVar(const char* name, const char* description);
	std::unordered_map<size_t, CVarParameter> cvarMap;
};

template<>
CVarArray<int32_t>* CVarImpl::GetCVarArray()
{
	return &this->intCVars;
}
template<>
CVarArray<std::string>* CVarImpl::GetCVarArray()
{
	return &this->stringCVars;
}
template<>
CVarArray<float>* CVarImpl::GetCVarArray()
{
	return &this->floatCVars;
}
template<>
CVarArray<bool>* CVarImpl::GetCVarArray()
{
	return &this->boolCVars;
}

/// <summary>
/// A Statically accessible instance of a CVAR, we will store all CVars inside this instance.
/// This is a singleton pattern.
/// </summary>
/// <returns>A Pointer to the CVar System.</returns>
CVar* CVar::Get()
{
	static CVarImpl instance;
	return &instance;
}

/// <summary>
/// Accessing a CVar by its hash value.
/// Typically you will want to use the BY_NAME macro to get the hash of a CVar by its name.
/// </summary>
/// <param name="hash"></param>
/// <returns></returns>
CVarParameter* CVarImpl::GetCVar(size_t hash)
{
	auto it = cvarMap.find(hash);

	if (it != cvarMap.end())
	{
		return &it->second;
	}
	//TODO Some logging here to say CVar not found.
	return nullptr; // CVar not found
}

CVarParameter* CVarImpl::CreateFloatCVar(const char* name, const char* description, float defaultValue, float maxValue, CVarFlags flags)
{
	CVarParameter* param = InitCVar(name, description);
	if (!param)
	{
		return nullptr; // CVar already exists
	}
	param->type = CVarType::FLOAT;

	GetCVarArray<float>()->Add(defaultValue, defaultValue, maxValue, param);
	return param;
}

CVarParameter* CVarImpl::CreateIntCVar(const char* name, const char* description, int32_t defaultValue, CVarFlags flags)
{
	CVarParameter* param = InitCVar(name, description);
	if (!param)
	{
		return nullptr; // CVar already exists
	}
	param->type = CVarType::INT;

	GetCVarArray<int>()->Add(defaultValue, param);
	return param;
}

CVarParameter* CVarImpl::CreateStringCVar(const char* name, const char* description, const char* defaultValue, CVarFlags flags)
{
	CVarParameter* param = InitCVar(name, description);
	if (!param)
	{
		return nullptr; // CVar already exists
	}
	param->type = CVarType::STRING;

	GetCVarArray<std::string>()->Add(defaultValue, param);
	return param;
}

CVarParameter* CVarImpl::CreateBoolCVar(const char* name, const char* description, bool defaultValue, CVarFlags flags)
{
	CVarParameter* param = InitCVar(name, description);
	if (!param)
	{
		return nullptr; // CVar already exists
	}
	param->type = CVarType::BOOL;

	GetCVarArray<bool>()->Add(defaultValue, param);
	return param;
}

float* CVarImpl::GetFloatCVar(size_t hash)
{
	return GetCVarCurrent<float>(hash);
}

void CVarImpl::SetFloatCVar(size_t hash, float value)
{
	SetCVarCurrent<float>(hash, value);
}

int* CVarImpl::GetIntCVar(size_t hash)
{
	return GetCVarCurrent<int>(hash);
}

void CVarImpl::SetIntCVar(size_t hash, int32_t value)
{
	SetCVarCurrent<int>(hash, value);
}

std::string* CVarImpl::GetStringCVar(size_t hash)
{
	return GetCVarCurrent<std::string>(hash);
}

void CVarImpl::SetStringCVar(size_t hash, const std::string& value)
{
	SetCVarCurrent<std::string>(hash, value);
}

bool* CVarImpl::GetBoolCVar(size_t hash)
{
	return GetCVarCurrent<bool>(hash);
}

void CVarImpl::SetBoolCVar(size_t hash, bool value)
{
	SetCVarCurrent<bool>(hash, value);
}

void CVarImpl::ImGuiDisplayCVars()
{
	ImGui::SeparatorText("Floats");
	for(int i = 0; i < floatCVars.lastCvarIndex; i++)
	{
		CVarParameter* param = floatCVars.cvars[i].parameters;	
		ImGui::SliderFloat(param->name.c_str(), &floatCVars.cvars[i].current, floatCVars.cvars[i].initial - floatCVars.cvars[i].max, floatCVars.cvars[i].max);
	}

	ImGui::SeparatorText("Integers");
	for (int i = 0; i < intCVars.lastCvarIndex; i++)
	{
		CVarParameter* param = intCVars.cvars[i].parameters;
		ImGui::InputInt(param->name.c_str(), &intCVars.cvars[i].current, 1, 100);
	}

	ImGui::SeparatorText("Strings");
	for (int i = 0; i < stringCVars.lastCvarIndex; i++)
	{
		CVarParameter* param = stringCVars.cvars[i].parameters;

		static int buf_size = 256;
		static char buf[256];

		std::string* str = &stringCVars.cvars[i].current;

		if (str->length() >= buf_size)
		{
			continue; // String too long to display; skipping.
		}

		stringCVars.cvars[i].current.copy(buf, buf_size);

		ImGui::InputText(param->name.c_str(), buf, buf_size);

		stringCVars.cvars[i].current = std::string(buf);
	}

	ImGui::SeparatorText("Booleans");
	for (int i = 0; i < boolCVars.lastCvarIndex; i++)
	{
		CVarParameter* param = boolCVars.cvars[i].parameters;
		ImGui::Checkbox(param->name.c_str(), &boolCVars.cvars[i].current);
	}
}

CVarParameter* CVarImpl::InitCVar(const char* name, const char* description)
{
	size_t nameHash = std::hash<std::string>{}(name);

	if (GetCVar(nameHash) != nullptr)
	{
		return nullptr; // CVar already exists
	}
	cvarMap[nameHash] = CVarParameter{};

	CVarParameter& param = cvarMap[nameHash];
	param.name = name;
	param.description = description;
	param.flags = CVarFlags::None; // Default flags

	return &param;
}

template<typename T>
T GetCVarCurrentByIndex(int32_t index)
{
	CVarImpl* cvarSystem = static_cast<CVarImpl*>(CVar::Get());
	return cvarSystem->GetCVarArray<T>()->GetCurrent(index);
}

template<typename T>
void SetCVarCurrentByIndex(int32_t index, T value)
{
	CVarImpl* cvarSystem = static_cast<CVarImpl*>(CVar::Get());
	return cvarSystem->GetCVarArray<T>()->SetCurrent(index, value);
}

AutoFloatCVar::AutoFloatCVar(const char* name, const char* description, float defaultValue, float maxValue, CVarFlags flags)
{
	maxValue = maxValue == FLT_MAX ? defaultValue * 10.0f : maxValue;
	CVarParameter* param = CVarImpl::Get()->CreateFloatCVar(name, description, defaultValue, maxValue, flags);
	param->flags = flags;
	index = param->arrayIndex;
}

float AutoFloatCVar::Get()
{
	return GetCVarCurrentByIndex<CVarType>(index);
}

void AutoFloatCVar::Set(float val)
{
	SetCVarCurrentByIndex<CVarType>(index, val);
}

AutoIntCVar::AutoIntCVar(const char* name, const char* description, int32_t defaultValue, CVarFlags flags)
{
	CVarParameter* param = CVarImpl::Get()->CreateIntCVar(name, description, defaultValue, flags);
	param->flags = flags;
	index = param->arrayIndex;
}

int32_t AutoIntCVar::Get()
{
	return GetCVarCurrentByIndex<CVarType>(index);
}

void AutoIntCVar::Set(int32_t val)
{
	SetCVarCurrentByIndex<CVarType>(index, val);
}

AutoStringCVar::AutoStringCVar(const char* name, const char* description, const char* defaultValue, CVarFlags flags)
{
	CVarParameter* param = CVarImpl::Get()->CreateStringCVar(name, description, defaultValue, flags);
	param->flags = flags;
	index = param->arrayIndex;
}

std::string AutoStringCVar::Get()
{
	return GetCVarCurrentByIndex<CVarType>(index);
}

void AutoStringCVar::Set(const std::string& val)
{
	SetCVarCurrentByIndex<CVarType>(index, val);
}

AutoBoolCVar::AutoBoolCVar(const char* name, const char* description, bool defaultValue, CVarFlags flags)
{
	CVarParameter* param = CVarImpl::Get()->CreateBoolCVar(name, description, defaultValue, flags);
	param->flags = flags;
	index = param->arrayIndex;
}

bool AutoBoolCVar::Get()
{
	return GetCVarCurrentByIndex<CVarType>(index);
}

void AutoBoolCVar::Set(bool val)
{
	SetCVarCurrentByIndex<CVarType>(index, val);
}