module;

#include "DiscordSDK/rapidjson/document.h"

export module Saturn.Generators.PickaxeGenerator;

import Saturn.Items.ItemModel;
import Saturn.Generators.BaseGenerator;

import <vector>;
import <string>;

export class FPickaxeGenerator : public FBaseGenerator {
public:
	static std::vector<FItem> GetItems();
	static std::vector<FItem> FilterItems(const std::string& filter);
	static FItem GetItemById(const std::string& id);
private:
	static rapidjson::Document json;
	static const constexpr char* ClassName = "AthenaPickaxeItemDefinition";
};