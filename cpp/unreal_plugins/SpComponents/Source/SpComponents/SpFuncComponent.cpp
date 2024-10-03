//
// Copyright(c) 2022 Intel. Licensed under the MIT License <http://opensource.org/licenses/MIT>.
//

#include "SpComponents/SpFuncComponent.h"

#include <functional> // std::function
#include <map>
#include <string>

#include <Containers/UnrealString.h> // FString
#include <Containers/Array.h>

#include "SpCore/FuncRegistrar.h"
#include "SpCore/Log.h"
#include "SpCore/SpFuncArray.h"
#include "SpCore/Unreal.h"

USpFuncComponent::USpFuncComponent()
{
    SP_LOG_CURRENT_FUNCTION();
}

USpFuncComponent::~USpFuncComponent()
{
    SP_LOG_CURRENT_FUNCTION();
}

void USpFuncComponent::registerFunc(const std::string& func_name, const std::function<SpFuncDataBundle(SpFuncDataBundle&)>& func)
{
    funcs_.registerFunc(func_name, func);

    #if WITH_EDITOR // defined in an auto-generated header
        FuncNames.Add(Unreal::toFString(func_name));
    #endif
}

void USpFuncComponent::unregisterFunc(const std::string& func_name)
{
    funcs_.unregisterFunc(func_name);

    #if WITH_EDITOR // defined in an auto-generated header
        FuncNames.Remove(Unreal::toFString(func_name));
    #endif
}

void USpFuncComponent::registerSharedMemoryView(const std::string& shared_memory_name, const SpFuncSharedMemoryView& shared_memory_view)
{
    SP_ASSERT(shared_memory_name != "");
    Std::insert(shared_memory_views_, shared_memory_name, shared_memory_view);

    #if WITH_EDITOR // defined in an auto-generated header
        SharedMemoryViewNames.Add(Unreal::toFString(shared_memory_name));
    #endif
}

void USpFuncComponent::unregisterSharedMemoryView(const std::string& shared_memory_name)
{
    SP_ASSERT(shared_memory_name != "");
    Std::remove(shared_memory_views_, shared_memory_name);

    #if WITH_EDITOR // defined in an auto-generated header
        SharedMemoryViewNames.Remove(Unreal::toFString(shared_memory_name));
    #endif
}

const std::map<std::string, SpFuncSharedMemoryView>& USpFuncComponent::getSharedMemoryViews() const
{
    return shared_memory_views_;
}

SpFuncDataBundle USpFuncComponent::callFunc(const std::string& func_name, SpFuncDataBundle& args) const
{
    return funcs_.call(func_name, args);
}
