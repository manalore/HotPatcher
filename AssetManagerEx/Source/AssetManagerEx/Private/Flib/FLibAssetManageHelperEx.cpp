// Fill out your copyright notice in the Description page of Project Settings.


#include "FLibAssetManageHelperEx.h"
#include "AssetManager/FAssetDependenciesInfo.h"
#include "AssetManager/FAssetDependenciesDetail.h"
#include "AssetManager/FFileArrayDirectoryVisitor.hpp"

#include "ARFilter.h"


FString UFLibAssetManageHelperEx::ConvVirtualToAbsPath(const FString& InPackagePath)
{
	FString ResultAbsPath;

	FString AssetAbsNotPostfix = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(UFLibAssetManageHelperEx::GetLongPackageNameFromPackagePath(InPackagePath)));
	FString AssetName = UFLibAssetManageHelperEx::GetAssetNameFromPackagePath(InPackagePath);
	FString SearchDir;
	{
		int32 FoundIndex;
		AssetAbsNotPostfix.FindLastChar('/', FoundIndex);
		if (FoundIndex != INDEX_NONE)
		{
			SearchDir = UKismetStringLibrary::GetSubstring(AssetAbsNotPostfix, 0, FoundIndex);
		}
	}

	TArray<FString> localFindFiles;
	IFileManager::Get().FindFiles(localFindFiles, *SearchDir, nullptr);

	for (const auto& Item : localFindFiles)
	{
		if (Item.Contains(AssetName) && Item[AssetName.Len()] == '.')
		{
			ResultAbsPath = FPaths::Combine(SearchDir, Item);
			break;
		}
	}

	return ResultAbsPath;
}


bool UFLibAssetManageHelperEx::ConvAbsToVirtualPath(const FString& InAbsPath, FString& OutPackagePath)
{
	bool runState = false;
	FString LongPackageName;
	runState = FPackageName::TryConvertFilenameToLongPackageName(InAbsPath, LongPackageName);

	if (runState)
	{
		FString PackagePath;
		if (UFLibAssetManageHelperEx::ConvLongPackageNameToPackagePath(LongPackageName, PackagePath))
		{
			OutPackagePath = PackagePath;
			runState = runState && true;
		}
	}
	
	return runState;
}

FString UFLibAssetManageHelperEx::GetLongPackageNameFromPackagePath(const FString& InPackagePath)
{
	FStringAssetReference InAssetRef = InPackagePath;
	return InAssetRef.GetLongPackageName();
}

FString UFLibAssetManageHelperEx::GetAssetNameFromPackagePath(const FString& InPackagePath)
{
	FStringAssetReference InAssetRef = InPackagePath;
	return InAssetRef.GetAssetName();
}


bool UFLibAssetManageHelperEx::ConvLongPackageNameToPackagePath(const FString& InLongPackageName, FString& OutPackagePath)
{
	OutPackagePath.Empty();
	bool runState = false;
	if (FPackageName::DoesPackageExist(InLongPackageName))
	{
		FString AssetName;
		{
			int32 FoundIndex;
			InLongPackageName.FindLastChar('/', FoundIndex);
			if (FoundIndex != INDEX_NONE)
			{
				AssetName = UKismetStringLibrary::GetSubstring(InLongPackageName, FoundIndex + 1, InLongPackageName.Len() - FoundIndex);
			}
		}
		OutPackagePath = InLongPackageName + TEXT(".") + AssetName;
		runState = true;
	}
	return runState;
}

bool UFLibAssetManageHelperEx::GetAssetPackageGUID(const FString& InPackagePath, FString& OutGUID)
{
	bool bResult = false;
	if (InPackagePath.IsEmpty())
		return false;

	const FAssetPackageData* AssetPackageData = UFLibAssetManageHelperEx::GetPackageDataByPackagePath(InPackagePath);
	if (AssetPackageData != NULL)
	{
		const FGuid& AssetGuid = AssetPackageData->PackageGuid;
		OutGUID = AssetGuid.ToString();
		bResult = true;
	}
	return bResult;
}


FAssetDependenciesInfo UFLibAssetManageHelperEx::CombineAssetDependencies(const FAssetDependenciesInfo& A, const FAssetDependenciesInfo& B)
{
	FAssetDependenciesInfo resault;

	auto CombineLambda = [&resault](const FAssetDependenciesInfo& InDependencies)
	{
		TArray<FString> Keys;
		InDependencies.mDependencies.GetKeys(Keys);
		for (const auto& Key : Keys)
		{
			if (!resault.mDependencies.Contains(Key))
			{
				resault.mDependencies.Add(Key, *InDependencies.mDependencies.Find(Key));
			}
			else
			{
				TArray<FString>& ExistingAssetList = resault.mDependencies.Find(Key)->mDependAsset;
				const TArray<FString>& PaddingAssetList = InDependencies.mDependencies.Find(Key)->mDependAsset;
				for (const auto& PaddingItem : PaddingAssetList)
				{
					if (!ExistingAssetList.Contains(PaddingItem))
					{
						ExistingAssetList.Add(PaddingItem);
					}
				}
			}
		}
	};

	CombineLambda(A);
	CombineLambda(B);

	return resault;
}

void UFLibAssetManageHelperEx::GetAssetDependencies(const FString& InLongPackageName, FAssetDependenciesInfo& OutDependices)
{
	if (InLongPackageName.IsEmpty())
		return;

	FStringAssetReference AssetRef = FStringAssetReference(InLongPackageName);
	if (!AssetRef.IsValid())
		return;
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	if (FPackageName::DoesPackageExist(InLongPackageName))
	{
		{
			TArray<FAssetData> AssetDataList;
			bool bResault = AssetRegistryModule.Get().GetAssetsByPackageName(FName(*InLongPackageName), AssetDataList);
			if (!bResault || !AssetDataList.Num())
			{
				UE_LOG(LogTemp, Error, TEXT("Faild to Parser AssetData of %s, please check."), *InLongPackageName);
				return;
			}
			if (AssetDataList.Num() > 1)
			{
				UE_LOG(LogTemp, Warning, TEXT("Got mulitple AssetData of %s,please check."), *InLongPackageName);
			}
		}
		UFlibAssetManageHelper::GatherAssetDependicesInfoRecursively(AssetRegistryModule, InLongPackageName, OutDependices);
	}
}

void UFLibAssetManageHelperEx::GetAssetListDependencies(const TArray<FString>& InLongPackageNameList, FAssetDependenciesInfo& OutDependices)
{
	FAssetDependenciesInfo result;

	for (const auto& LongPackageItem : InLongPackageNameList)
	{
		FAssetDependenciesInfo CurrentDependency;
		UFLibAssetManageHelperEx::GetAssetDependencies(LongPackageItem, CurrentDependency);
		result = UFLibAssetManageHelperEx::CombineAssetDependencies(result, CurrentDependency);
	}
	OutDependices = result;
}

void UFLibAssetManageHelperEx::GatherAssetDependicesInfoRecursively(
	FAssetRegistryModule& InAssetRegistryModule,
	const FString& InTargetLongPackageName,
	FAssetDependenciesInfo& OutDependencies
)
{
	TArray<FName> local_Dependencies;
	bool bGetDependenciesSuccess = InAssetRegistryModule.Get().GetDependencies(FName(*InTargetLongPackageName), local_Dependencies, EAssetRegistryDependencyType::Packages);
	if (bGetDependenciesSuccess)
	{
		for (auto &DependItem : local_Dependencies)
		{
			FString LongDependentPackageName = DependItem.ToString();
			FString BelongModuleName = UFLibAssetManageHelperEx::GetAssetBelongModuleName(LongDependentPackageName);

			// add a new asset to module category
			auto AddNewAssetItemLambda = [&InAssetRegistryModule, &OutDependencies](
				FAssetDependenciesDetail& ModuleAssetDependDetail,
				FString AssetPackageName)
			{
				if (ModuleAssetDependDetail.mDependAsset.Find(AssetPackageName) == INDEX_NONE)
				{
					ModuleAssetDependDetail.mDependAsset.Add(AssetPackageName);
					GatherAssetDependicesInfoRecursively(InAssetRegistryModule, AssetPackageName, OutDependencies);
				}
			};

			if (OutDependencies.mDependencies.Contains(BelongModuleName))
			{
				// UE_LOG(LogTemp, Log, TEXT("Belong Module is %s,Asset is %s"), *BelongModuleName, *LongDependentPackageName);
				FAssetDependenciesDetail* ModuleCategory = OutDependencies.mDependencies.Find(BelongModuleName);
				AddNewAssetItemLambda(*ModuleCategory, LongDependentPackageName);
			}
			else
			{
				// UE_LOG(LogTemp, Log, TEXT("New Belong Module is %s,Asset is %s"), *BelongModuleName,*LongDependentPackageName);
				FAssetDependenciesDetail& NewModuleCategory = OutDependencies.mDependencies.Add(BelongModuleName, FAssetDependenciesDetail{});
				NewModuleCategory.mModuleCategory = BelongModuleName;
				AddNewAssetItemLambda(NewModuleCategory, LongDependentPackageName);
			}
		}
	}
}

bool UFLibAssetManageHelperEx::GetModuleAssetsList(const FString& InModuleName, TArray<FString>& OutAssetList)
{
	TArray<FAssetData> AllAssetData;
	if (UFLibAssetManageHelperEx::GetModuleAssets(InModuleName, AllAssetData))
	{
		for (const auto& AssetDataIndex : AllAssetData)
		{
			OutAssetList.Add(AssetDataIndex.PackageName.ToString());
		}
		return true;
	}
	return false;
}

bool UFLibAssetManageHelperEx::GetModuleAssets(const FString& InModuleName, TArray<FAssetData>& OutAssetData)
{
	OutAssetData.Reset();
	TArray<FString> AllEnableModule;
	UFLibAssetManageHelperEx::GetAllEnabledModuleName(AllEnableModule);
	
	if (!AllEnableModule.Contains(InModuleName))
		return false;

	FARFilter Filter;
	Filter.bIncludeOnlyOnDiskAssets = true;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.AddUnique(*(TEXT("/") + InModuleName));

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().GetAssets(Filter, OutAssetData);
	
	return true;
}

const FAssetPackageData* UFLibAssetManageHelperEx::GetPackageDataByPackagePath(const FString& InPackagePath)
{

	if (InPackagePath.IsEmpty())
		return NULL;
	if (!InPackagePath.IsEmpty())
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		FString TargetLongPackageName = UFLibAssetManageHelperEx::GetLongPackageNameFromPackagePath(InPackagePath);

		if(FPackageName::DoesPackageExist(TargetLongPackageName))
		{
			FAssetPackageData* AssetPackageData = const_cast<FAssetPackageData*>(AssetRegistryModule.Get().GetAssetPackageData(*TargetLongPackageName));
			if (AssetPackageData != nullptr)
			{
				return AssetPackageData;
			}
		}
	}

	return NULL;
}


/*
	TOOLs Set Implementation
*/

bool UFLibAssetManageHelperEx::SerializeAssetDependenciesToJson(const FAssetDependenciesInfo& InAssetDependencies, FString& OutJsonStr)
{
	OutJsonStr.Empty();
	TSharedPtr<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject);
	{
		// collect all module name
		TArray<FString> AssetCategoryList;
		InAssetDependencies.mDependencies.GetKeys(AssetCategoryList);
		{
			TArray<TSharedPtr<FJsonValue>> JsonCategoryList;
			for (const auto& AssetCategoryItem : AssetCategoryList)
			{
				JsonCategoryList.Add(MakeShareable(new FJsonValueString(AssetCategoryItem)));
			}
			RootJsonObject->SetArrayField(JSON_MODULE_LIST_SECTION_NAME, JsonCategoryList);
		}
		// collect all invalid asset
		{
			TArray<FString> AllInValidAssetList;
			UFlibAssetManageHelper::GetAllInValidAssetInProject(UKismetSystemLibrary::GetProjectDirectory(), InAssetDependencies, AllInValidAssetList);
			if (AllInValidAssetList.Num() > 0)
			{
				TArray<TSharedPtr<FJsonValue>> JsonInvalidAssetList;
				for (const auto& InValidAssetItem : AllInValidAssetList)
				{
					JsonInvalidAssetList.Add(MakeShareable(new FJsonValueString(InValidAssetItem)));
				}
				RootJsonObject->SetArrayField(JSON_ALL_INVALID_ASSET_SECTION_NAME, JsonInvalidAssetList);
			}
		}

		// serialize asset list
		for (const auto& AssetCategoryItem : AssetCategoryList)
		{
			{
				// TSharedPtr<FJsonObject> CategoryJsonObject = MakeShareable(new FJsonObject);

				TArray<TSharedPtr<FJsonValue>> CategoryAssetListJsonEntity;
				const FAssetDependenciesDetail& CategortItem = InAssetDependencies.mDependencies[AssetCategoryItem];
				for (const auto& AssetItem : CategortItem.mDependAsset)
				{
					CategoryAssetListJsonEntity.Add(MakeShareable(new FJsonValueString(AssetItem)));
				}
				// CategoryJsonObject->SetArrayField(AssetCategoryItem, CategoryAssetListJsonEntity);

				RootJsonObject->SetArrayField(AssetCategoryItem, CategoryAssetListJsonEntity);
			}
		}
	}

	auto JsonWriter = TJsonWriterFactory<>::Create(&OutJsonStr);
	FJsonSerializer::Serialize(RootJsonObject.ToSharedRef(), JsonWriter);
	return true;
}


bool UFLibAssetManageHelperEx::DeserializeAssetDependencies(const FString& InStream, FAssetDependenciesInfo& OutAssetDependencies)
{
	if (InStream.IsEmpty()) return false;

	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(InStream);
	TSharedPtr<FJsonObject> JsonObject;;
	if (FJsonSerializer::Deserialize(JsonReader, JsonObject))
	{
		TArray<TSharedPtr<FJsonValue>> JsonModuleList = JsonObject->GetArrayField(JSON_MODULE_LIST_SECTION_NAME);
		for (const auto& JsonModuleItem : JsonModuleList)
		{

			FString ModuleName = JsonModuleItem->AsString();

			TArray<TSharedPtr<FJsonValue>> JsonAssetList = JsonObject->GetArrayField(ModuleName);
			TArray<FString> AssetList;

			for (const auto& JsonAssetItem : JsonAssetList)
			{
				FString AssetInfo = JsonAssetItem->AsString();
				AssetList.Add(AssetInfo);
			}
			OutAssetDependencies.mDependencies.Add(ModuleName, FAssetDependenciesDetail{ ModuleName,AssetList });
		}
	}
	return true;
}

bool UFLibAssetManageHelperEx::SaveStringToFile(const FString& InFile, const FString& InString)
{
	return FFileHelper::SaveStringToFile(InString, *InFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool UFLibAssetManageHelperEx::LoadFileToString(const FString& InFile, FString& OutString)
{
	return FFileHelper::LoadFileToString(OutString, *InFile);
}

bool UFLibAssetManageHelperEx::GetPluginModuleAbsDir(const FString& InPluginModuleName, FString& OutPath)
{
	bool bFindResault = false;
	TSharedPtr<IPlugin> FoundModule = IPluginManager::Get().FindPlugin(InPluginModuleName);

	if (FoundModule.IsValid())
	{
		bFindResault = true;
		OutPath = FPaths::ConvertRelativePathToFull(FoundModule->GetBaseDir());
	}
	return bFindResault;
}

void UFLibAssetManageHelperEx::GetAllEnabledModuleName(TArray<FString>& OutEnabledModule)
{
	OutEnabledModule.Reset();
	OutEnabledModule.Add(TEXT("Game"));
	OutEnabledModule.Add(TEXT("Engine"));
	TArray<TSharedRef<IPlugin>> AllPlugin = IPluginManager::Get().GetEnabledPlugins();

	for (const auto& PluginItem : AllPlugin)
	{
		OutEnabledModule.Add(PluginItem.Get().GetName());
	}
}

bool UFLibAssetManageHelperEx::GetEnableModuleAbsDir(const FString& InModuleName, FString& OutPath)
{
	if (InModuleName.Equals(TEXT("Engine")))
	{
		OutPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
		return true;
	}
	if (InModuleName.Equals(TEXT("Game")))
	{
		OutPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		return true;
	}
	return UFlibAssetManageHelper::GetPluginModuleAbsDir(InModuleName, OutPath);

}

FString UFLibAssetManageHelperEx::GetAssetBelongModuleName(const FString& InAssetRelativePath)
{
	FString LongDependentPackageName = InAssetRelativePath;

	int32 BelongModuleNameEndIndex = LongDependentPackageName.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, 1);
	FString BelongModuleName = UKismetStringLibrary::GetSubstring(LongDependentPackageName, 1, BelongModuleNameEndIndex - 1);// (LongDependentPackageName, BelongModuleNameEndIndex);

	return BelongModuleName;
}

bool UFLibAssetManageHelperEx::IsValidPlatform(const FString& PlatformName)
{
	for (const auto& PlatformItem : UFlibAssetManageHelper::GetAllTargetPlatform())
	{
		if (PlatformItem.Equals(PlatformName))
		{
			return true;
		}
	}
	return false;
}

TArray<FString> UFLibAssetManageHelperEx::GetAllTargetPlatform()
{
#ifdef __DEVELOPER_MODE__
	TArray<ITargetPlatform*> Platforms = GetTargetPlatformManager()->GetTargetPlatforms();
	TArray<FString> result;

	for (const auto& PlatformItem : Platforms)
	{
		result.Add(PlatformItem->PlatformName());
	}

#else
	TArray<FString> result = {
		"AllDesktop",
		"MacClient",
		"MacNoEditor",
		"MacServer",
		"Mac",
		"WindowsClient",
		"WindowsNoEditor",
		"WindowsServer",
		"Windows",
		"Android",
		"Android_ASTC",
		"Android_ATC",
		"Android_DXT",
		"Android_ETC1",
		"Android_ETC1a",
		"Android_ETC2",
		"Android_PVRTC",
		"AndroidClient",
		"Android_ASTCClient",
		"Android_ATCClient",
		"Android_DXTClient",
		"Android_ETC1Client",
		"Android_ETC1aClient",
		"Android_ETC2Client",
		"Android_PVRTCClient",
		"Android_Multi",
		"Android_MultiClient",
		"HTML5",
		"IOSClient",
		"IOS",
		"TVOSClient",
		"TVOS",
		"LinuxClient",
		"LinuxNoEditor",
		"LinuxServer",
		"Linux",
		"Lumin",
		"LuminClient"
	};

#endif
	return result;
}


bool UFLibAssetManageHelperEx::FindFilesRecursive(const FString& InStartDir, TArray<FString>& OutFileList, bool InRecursive)
{
	TArray<FString> CurrentFolderFileList;
	if (!FPaths::DirectoryExists(InStartDir))
		return false;

	FFillArrayDirectoryVisitor FileVisitor;
	IFileManager::Get().IterateDirectoryRecursively(*InStartDir, FileVisitor);

	OutFileList.Append(FileVisitor.Files);

	return true;
}

FString UFLibAssetManageHelperEx::ConvPath_Slash2BackSlash(const FString& InPath)
{
	FString ResaultPath;
	TArray<FString> OutArray;

	InPath.ParseIntoArray(OutArray, TEXT("/"));
	int32 OutArrayNum = OutArray.Num();
	for (int32 Index = 0; Index < OutArrayNum; ++Index)
	{
		if (!OutArray[Index].IsEmpty() && Index < OutArrayNum - 1)/* && FPaths::DirectoryExists(ResaultPath + item)*/
		{
			ResaultPath.Append(OutArray[Index]);
			ResaultPath.Append(TEXT("\\"));
		}
		else {
			ResaultPath.Append(OutArray[Index]);
		}
	}
	return ResaultPath;
}

FString UFLibAssetManageHelperEx::ConvPath_BackSlash2Slash(const FString& InPath)
{
	FString ResaultPath;
	TArray<FString> OutArray;
	InPath.ParseIntoArray(OutArray, TEXT("\\"));
	if (OutArray.Num() == 1 && OutArray[0] == InPath)
	{
		InPath.ParseIntoArray(OutArray, TEXT("/"));
	}
	int32 OutArrayNum = OutArray.Num();
	for (int32 Index = 0; Index < OutArrayNum; ++Index)
	{
		if (!OutArray[Index].IsEmpty() && Index < OutArrayNum - 1)/* && FPaths::DirectoryExists(ResaultPath + item)*/
		{
			ResaultPath.Append(OutArray[Index]);
			ResaultPath.Append(TEXT("/"));
		}
		else {
			ResaultPath.Append(OutArray[Index]);
		}
	}
	return ResaultPath;
}