import Saturn.Readers.ZenPackageReader;

#include "Saturn/Log.h"
#include "Saturn/Defines.h"

import <string>;
import <vector>;
import <cstdint>;
import <iomanip>;

import Saturn.Core.UObject;
import Saturn.Core.IoStatus;
import Saturn.Structs.Name;
import Saturn.Asset.NameMap;
import Saturn.Misc.IoBuffer;
import Saturn.Asset.PackageIndex;
import Saturn.Core.GlobalContext;
import Saturn.Readers.MemoryReader;
import Saturn.Asset.ExportMapEntry;
import Saturn.Unversioned.Fragment;
import Saturn.Asset.BulkDataMapEntry;
import Saturn.Core.LazyPackageObject;
import Saturn.Asset.ExportBundleEntry;
import Saturn.Asset.PackageObjectIndex;
import Saturn.Asset.DependencyBundleEntry;
import Saturn.ZenPackage.ZenPackageHeader;
import Saturn.Asset.DependencyBundleHeader;
import Saturn.ZenPackage.ZenPackageSummary;
import Saturn.Unversioned.UnversionedHeader;

UZenPackage::UZenPackage(FZenPackageHeader& InHeader, TSharedPtr<GlobalContext>& InContext) {
    Name = std::string(InHeader.PackageName.begin(), InHeader.PackageName.end());
    Context = InContext;
}

void UZenPackage::ProcessExports(FZenPackageData& PackageData) {
    PackageData.Exports.resize(PackageData.Header.ExportCount);

    for (size_t i = 0; i < PackageData.Exports.size(); i++) {
        if (!PackageData.Exports[i].Object) {
            PackageData.Exports[i].Object = std::make_shared<UObject>();
        }
    }

    auto& Header = PackageData.Header;
    auto ExportOffset = 0;

    for (size_t i = 0; i < Header.ExportBundleEntries.size(); i++) {
        auto& ExportBundle = Header.ExportBundleEntries[i];
        auto& LocalExport = PackageData.Exports[ExportBundle.LocalExportIndex];

        if (ExportBundle.CommandType == FExportBundleEntry::ExportCommandType_Create) {
            CreateExport(PackageData, PackageData.Exports, ExportBundle.LocalExportIndex);
            continue;
        }

        if (ExportBundle.CommandType != FExportBundleEntry::ExportCommandType_Serialize)
            continue; // the only other option is count (which obv will not be used, so this is more of making sure we read the right value

        auto Export = TrySerializeExport(PackageData, ExportBundle.LocalExportIndex);

        if (Export.has_value()) {
            Exports.push_back(Export.value());
        }
    }
}

void UZenPackage::CreateExport(FZenPackageData& PackageData, std::vector<FExportObject>& Exports, int32_t LocalExportIndex) {
    auto& Header = PackageData.Header;
    auto& Export = Header.ExportMap[LocalExportIndex];
    auto ObjectNameW = Header.NameMap.GetName(Export.ObjectName);
    auto ObjectName = std::string(ObjectNameW.begin(), ObjectNameW.end());

    bool IsTargetObject = ObjectName == PackageData.ExportState.TargetObjectName;

    if (IsTargetObject) {
        Exports[LocalExportIndex].Object = PackageData.ExportState.TargetObject;
    }
    else if (PackageData.ExportState.LoadTargetOnly) {
        return;
    }

    UObjectPtr& Object = Exports[LocalExportIndex].Object;
    auto& TemplateObject = Exports[LocalExportIndex].TemplateObject;

    TemplateObject = IndexToObject(Header, Exports, Export.TemplateIndex);

    if (!TemplateObject) {
        PackageData.Reader.Status = FIoStatus(EIoErrorCode::ReadError, "Template object could not be loaded for FZenPackage.");
        return;
    }

    Object->Name = std::string(ObjectName.begin(), ObjectName.end());

    if (!Object->Class) {
        Object->Class = IndexToObject<UClass>(Header, Exports, Export.ClassIndex).As<UClass>();
    }

    if (!Object->Outer) {
        Object->Outer = Export.OuterIndex.IsNull() ? This() : IndexToObject(Header, Exports, Export.OuterIndex);
    }

    if (UStructPtr Struct = Object.As<UStruct>()) {
        if (!Struct->GetSuper()) {
            Struct->SetSuper(IndexToObject<UStruct>(Header, Exports, Export.SuperIndex).As<UStruct>());
        }
    }

    Object->ObjectFlags = UObject::EObjectFlags(Export.ObjectFlags | UObject::EObjectFlags::RF_NeedLoad | UObject::EObjectFlags::RF_NeedPostLoad | UObject::EObjectFlags::RF_NeedPostLoadSubobjects | UObject::EObjectFlags::RF_WasLoaded);
}

std::optional<UObjectPtr> UZenPackage::TrySerializeExport(FZenPackageData& PackageData, int32_t LocalExportIndex) {
    auto& Export = PackageData.Header.ExportMap[LocalExportIndex];
    auto& ExportObject = PackageData.Exports[LocalExportIndex];
    UObjectPtr& Object = ExportObject.Object;

    if (PackageData.ExportState.LoadTargetOnly and Object != PackageData.ExportState.TargetObject)
        return std::nullopt;

    Object->ClearFlags(UObject::RF_NeedLoad);

    Object->Serialize(PackageData.Reader);

    return Object;
}

template <typename T = UObject>
TObjectPtr<T> UZenPackage::CreateScriptObject(TSharedPtr<GlobalContext> Context, FPackageObjectIndex& Index) {
    if (!Context->GlobalToc->ScriptObjectByGlobalIdMap.contains(Index)) {
        LOG_ERROR("Failed to find script object with index {0}. ScriptMap has a size of {1}.", GetTypeHash(Index), Context->GlobalToc->ScriptObjectByGlobalIdMap.size());
        return nullptr;
    }

    auto ScriptObject = Context->GlobalToc->ScriptObjectByGlobalIdMap[Index];
    std::wstring NameW = Context->GlobalToc->NameMap.GetName(ScriptObject.MappedName);
    std::string Name = std::string(NameW.begin(), NameW.end());

    if (Context->ObjectArray.contains(Name)) {
        UObjectPtr Ret = Context->ObjectArray[Name];

        if (!Ret->GetOuter() && !ScriptObject.OuterIndex.IsNull()) {
            Ret->SetOuter(CreateScriptObject<UObject>(Context, ScriptObject.OuterIndex));
        }

        return Ret.As<T>();
    }

    auto Ret = std::make_shared<T>();
    Ret->SetName(Name);

    if (!ScriptObject.OuterIndex.IsNull()) {
        Ret->SetOuter(CreateScriptObject<UObject>(Context, ScriptObject.OuterIndex));
    }

    Ret->SetFlags(UObject::RF_NeedLoad);

    return Ret;
}

template <typename T = UObject>
UObjectPtr UZenPackage::IndexToObject(FZenPackageHeader& Header, std::vector<FExportObject>& Exports, FPackageObjectIndex Index) {
    if (Index.IsNull()) {
        return {};
    }

    if (Index.IsExport()) {
        return Exports[Index.ToExport()].Object;
    }

    if (Index.IsImport()) {
        if (Index.IsScriptImport()) {
            auto ContextLock = Context.lock();

            if (!ContextLock) {
                return nullptr;
            }

            auto Ret = CreateScriptObject<T>(ContextLock, Index);

            if (!ContextLock->ObjectArray.contains(Ret->GetName())) {
                ContextLock->ObjectArray.insert_or_assign(Ret->GetName(), Ret.As<UObject>());
            }

            return Ret.As<UObject>();
        }
        else if (Index.IsPackageImport()) {
            if (Index.GetImportedPackageIndex() >= Header.ImportedPackageIds.size()) {
                return {};
            }

            FPackageId PackageId = Header.ImportedPackageIds[Index.GetImportedPackageIndex()];

            return UObjectPtr(std::make_shared<ULazyPackageObject>(PackageId));
        }
    }

    return {};
}

FIoStatus& FZenPackageReader::GetStatus() {
    return Status;
}

bool FZenPackageReader::IsOk() {
    return Status.IsOk();
}

UPackagePtr FZenPackageReader::MakePackage(TSharedPtr<GlobalContext> Context, FExportState& ExportState) {
    PackageData = std::make_shared<FZenPackageData>();
    Package = PackageData->Package = std::make_shared<UZenPackage>(PackageHeader, Context);
    PackageData->ExportState = ExportState;
    PackageData->Header = PackageHeader;
    PackageData->Reader = *this;
    PackageData->Reader.Seek(PackageHeader.ExportOffset);

    Package->ProcessExports(*PackageData);

    return Package.As<UPackage>();
}

void FZenPackageReader::LoadProperties(UStructPtr Struct, UObjectPtr Object) {
    FUnversionedHeader header;
    FIoStatus status = header.Load(*this);
    if (!status.IsOk()) {
        LOG_ERROR("Error reading FUnversioned header: {0}", status.ToString());
        return;
    }

    if (!header.HasNonZeroValues() or !header.HasValues()) {
        LOG_TRACE("Provided asset either doesn't have NonZero values or doesn't have values at all.");
        return;
    }

    for (FUnversionedIterator It(header, Struct); It; It.Next()) {
        if (!It.IsNonZero()) continue;

        FProperty* Prop = *It;

        LOG_TRACE("Loading Property [Name: ({0}), Pos: ({1})]", Prop->GetName(), Tell());

        TUniquePtr<IPropValue> Value = Prop->Serialize(*this);

        if (!Value) continue;

        Object->PropertyValues.push_back({ Prop->Name, std::move(Value) });
    }
}

uint32_t FZenPackageReader::GetCookedHeaderSize() {
    return PackageHeader.CookedHeaderSize;
}

uint32_t FZenPackageReader::GetExportCount() {
    return PackageHeader.ExportCount;
}

FNameMap& FZenPackageReader::GetNameMap() {
    return PackageHeader.NameMap;
}

std::wstring& FZenPackageReader::GetPackageName() {
    return PackageHeader.PackageName;
}

FZenPackageSummary* FZenPackageReader::GetPackageSummary() {
    return PackageHeader.PackageSummary;
}

std::vector<uint64_t>& FZenPackageReader::GetImportedPublicExportHashes() {
    return PackageHeader.ImportedPublicExportHashes;
}

std::vector<FPackageObjectIndex>& FZenPackageReader::GetImportMap() {
    return PackageHeader.ImportMap;
}

std::vector<FExportMapEntry>& FZenPackageReader::GetExportMap() {
    return PackageHeader.ExportMap;
}

std::vector<FBulkDataMapEntry>& FZenPackageReader::GetBulkDataMap() {
    return PackageHeader.BulkDataMap;
}

std::vector<FExportBundleEntry>& FZenPackageReader::GetExportBundleEntries() {
    return PackageHeader.ExportBundleEntries;
}

std::vector<FDependencyBundleHeader>& FZenPackageReader::GetDependencyBundleHeaders() {
    return PackageHeader.DependencyBundleHeaders;
}

std::vector<FDependencyBundleEntry>& FZenPackageReader::GetDependencyBundleEntries() {
    return PackageHeader.DependencyBundleEntries;
}

std::vector<std::wstring>& FZenPackageReader::GetImportedPackageNames() {
    return PackageHeader.ImportedPackageNames;
}

std::vector<uint8_t> FZenPackageReader::SerializeAsByteArray(std::vector<uint8_t>& Original) {
    FZenPackageReader otherReader(Original);
    int32_t Difference = FNameMap::GetNameMapByteDifference(otherReader.GetNameMap(), GetNameMap());

    std::vector<uint8_t> buffer;
    buffer.resize(sizeof(FZenPackageSummary));
    memcpy(buffer.data(), Original.data(), sizeof(FZenPackageSummary));

    PackageHeader.NameMap.SaveToBuffer(buffer);

    FZenPackageSummary* Summary = reinterpret_cast<FZenPackageSummary*>(buffer.data());
    Summary->HeaderSize -= Difference;
    Summary->ImportedPublicExportHashesOffset -= Difference;
    Summary->ImportMapOffset -= Difference;
    Summary->ExportMapOffset -= Difference;
    Summary->ExportBundleEntriesOffset -= Difference;
    Summary->DependencyBundleHeadersOffset -= Difference;
    Summary->DependencyBundleEntriesOffset -= Difference;
    Summary->ImportedPackageNamesOffset -= Difference;

    std::vector<uint8_t> OtherNameMap;
    otherReader.GetNameMap().SaveToBuffer(OtherNameMap);

    Original.erase(Original.begin(), Original.begin() + sizeof(FZenPackageSummary) + OtherNameMap.size());

    buffer.append_range(Original);

    return buffer;
}

FZenPackageReader& operator<<(FZenPackageReader& Ar, UObjectPtr& Object) {
    FPackageIndex Index;
    Ar << Index;

    if (Index.IsNull()) {
        Object = UObjectPtr(nullptr);
        Object->Index = std::make_shared<FPackageIndex>(Index);
        return Ar;
    }

    if (Index.IsExport()) {
        int32_t ExportIndex = Index.ToExport();
        if (ExportIndex < Ar.PackageData->Exports.size()) {
            Object = Ar.PackageData->Exports[ExportIndex].Object;
            Object->Index = std::make_shared<FPackageIndex>(Index);
        }
        else {
            Ar.Status = FIoStatus(EIoErrorCode::ReadError, "Export index read is not a valid index.");
        }

        return Ar;
    }

    auto& ImportMap = Ar.PackageData->Header.ImportMap;

    if (Index.IsImport() && Index.ToImport() < ImportMap.size()) {
        Object = Ar.PackageData->Package->IndexToObject(Ar.PackageData->Header, Ar.PackageData->Exports, Ar.PackageData->Header.ImportMap[Index.ToImport()]);
        Object->Index = std::make_shared<FPackageIndex>(Index);
    }
    else {
        Ar.Status = FIoStatus(EIoErrorCode::ReadError, "Bad object import index.");
    }

    return Ar;
}

FZenPackageReader& operator>>(FZenPackageReader& Ar, UObjectPtr& Object) {
    Ar >> *Object->Index;

    return Ar;
}

FZenPackageReader& operator<<(FZenPackageReader& Ar, UStructPtr& Struct) {
    return Ar << reinterpret_cast<UObjectPtr&>(Struct);
}

FZenPackageReader& operator>>(FZenPackageReader& Ar, UStructPtr& Struct) {
    return Ar >> reinterpret_cast<UObjectPtr&>(Struct);
}

FZenPackageReader& operator<<(FZenPackageReader& Ar, FName& Name) {
    uint32_t NameIndex;
    Ar << NameIndex;
    uint32_t Number = 0;
    Ar << Number;

    auto MappedName = FMappedName::Create(NameIndex, Number, FMappedName::EType::Package);
    auto NameStrW = Ar.PackageHeader.NameMap.GetName(MappedName);

    if (NameStrW.empty()) {
        LOG_WARN("Name serialized is empty or invalid.");
    }

    std::string NameStr = std::string(NameStrW.begin(), NameStrW.end());
    Name = NameStr;

    return Ar;
}

bool endsWithNumber(const std::string& str, int& number) {
    if (str.empty() || !std::isdigit(str.back())) {
        return false;
    }

    size_t pos = str.rfind('_');
    if (pos == std::string::npos || pos == str.size() - 1) {
        return false;
    }

    std::string numberStr = str.substr(pos + 1);
    if (numberStr.empty() || !std::all_of(numberStr.begin(), numberStr.end(), ::isdigit)) {
        return false;
    }

    number = std::stoi(numberStr);
    return true;
}

FZenPackageReader& operator>>(FZenPackageReader& Ar, FName& Name) {
    int number = 0;
    if (endsWithNumber(Name.ToString(), number)) {
        number--;
    }

    int index = 0;
    bool bFound = false;
    for (auto& nameW : Ar.PackageHeader.NameMap) {
        std::string name = std::string(nameW.begin(), nameW.end());
        if (name == Name.ToString()) {
            bFound = true;
            break;
        }
        index++;
    }

    if (!bFound) {
        LOG_WARN("Failed to find name {0} in Name Map", Name.ToString());
    }

    Ar >> index;
    Ar >> number;

    return Ar;
}
