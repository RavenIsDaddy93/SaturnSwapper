module;

#include "Saturn/Defines.h"
#include "tsl/ordered_set.h"

export module Saturn.Properties.SetProperty;

import Saturn.Readers.ZenPackageReader;
export import Saturn.Reflection.FProperty;

template <typename K>
using TOrderedSet = tsl::ordered_set<K>;

export class FSetProperty : public FProperty {
public:
    friend class FPropertyFactory;
private:
    FProperty* ElementType;
public:
    struct Value : public IPropValue {
    public:
        TSharedPtr<class IPropValue> Val;

        __forceinline bool IsAcceptableType(EPropertyType Type) override {
            return Type == EPropertyType::SetProperty;
        }

        __forceinline void PlaceValue(EPropertyType Type, void* OutBuffer) override {
            
        }

        void Write(FZenPackageReader& Ar, ESerializationMode SerializationMode = ESerializationMode::Normal) override {
            Val->Write(Ar);
        }
    };

    TUniquePtr<class IPropValue> Serialize(FZenPackageReader& Ar) override {
        auto Ret = std::make_unique<Value>();
        Ret->Val = ElementType->Serialize(Ar);

        return std::move(Ret);
    }

    __forceinline FProperty* GetElementType() {
        return ElementType;
    }
};