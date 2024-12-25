module;

#include "Saturn/Defines.h"

export module Saturn.Properties.MapProperty;

export import Saturn.Reflection.FProperty;
import Saturn.Readers.FArchive;

export class FMapProperty : public FProperty {
public:
    friend class FPropertyFactory;
private:
    FProperty* KeyType;
    FProperty* ValueType;
public:
    struct Value : public IPropValue {
    public:
        std::vector<TSharedPtr<class IPropValue>> KeysToRemove;
        TMap<TSharedPtr<class IPropValue>, TSharedPtr<class IPropValue>> Value;

        __forceinline bool IsAcceptableType(EPropertyType Type) override {
            return Type == EPropertyType::MapProperty;
        }

        __forceinline void PlaceValue(EPropertyType Type, void* OutBuffer) override {
            
        }

        void Write(FArchive& Ar, ESerializationMode SerializationMode = ESerializationMode::Normal) override {
            Ar >> static_cast<uint32_t>(KeysToRemove.size());
            for (TSharedPtr<IPropValue> key : KeysToRemove) {
                key->Write(Ar);
            }

            Ar >> static_cast<uint32_t>(Value.size());
            for (auto& kvp : Value) {
                kvp.first->Write(Ar);
                kvp.second->Write(Ar);
            }
        }
    };

    TUniquePtr<class IPropValue> Serialize(FArchive& Ar) override {
        auto Ret = std::make_unique<Value>();

        int32_t NumKeysToRemove = 0;
        Ar << NumKeysToRemove;

        for (; NumKeysToRemove; --NumKeysToRemove) {
            TUniquePtr<IPropValue> keyToRemove = KeyType->Serialize(Ar);
            Ret->KeysToRemove.push_back(std::move(keyToRemove));
        }

        int32_t NumEntries = 0;
        Ar << NumEntries;

        for (; NumEntries; --NumEntries) {
            TUniquePtr<IPropValue> key = KeyType->Serialize(Ar);
            TUniquePtr<IPropValue> value = ValueType->Serialize(Ar);
            Ret->Value.insert({ std::move(key), std::move(value) });
        }

        return std::move(Ret);
    }

    __forceinline FProperty* GetKeyProp() {
        return KeyType;
    }

    __forceinline FProperty* GetValueProp() {
        return ValueType;
    }
};