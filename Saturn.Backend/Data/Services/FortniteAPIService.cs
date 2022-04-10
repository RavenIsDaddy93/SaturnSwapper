﻿using Newtonsoft.Json;
using Saturn.Backend.Data.Enums;
using Saturn.Backend.Data.Models.FortniteAPI;
using Saturn.Backend.Data.Models.Items;
using Saturn.Backend.Data.Utils;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Threading.Tasks;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using CUE4Parse.FileProvider;
using CUE4Parse.UE4.Assets.Exports;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse.UE4.Objects.UObject;
using MudBlazor;
using Serilog;
using SkiaSharp;

namespace Saturn.Backend.Data.Services
{
    public interface IFortniteAPIService
    {
        public Task<List<Cosmetic>> GetSaturnMisc();
        public Task<List<Cosmetic>> RemoveItems(List<Cosmetic> items);
        public Models.FortniteAPI.Data GetAES();
        public Task<List<Cosmetic>> AreItemsConverted(List<Cosmetic> items);
        public Task<List<Cosmetic>> AddExtraItems(List<Cosmetic> items, ItemType itemType);
    }

    public class FortniteAPIService : IFortniteAPIService
    {
        private readonly IConfigService _configService;
        private readonly IDiscordRPCService _discordRPCService;
        private readonly ICloudStorageService _cloudStorageService;


        private readonly Uri Base = new("https://fortnite-api.com/v2/");


        public FortniteAPIService(IConfigService configService, IDiscordRPCService discordRPCService, ICloudStorageService cloudStorageService)
        {
            _configService = configService;
            _discordRPCService = discordRPCService;
            _cloudStorageService = cloudStorageService;
        }

        private Uri AES => new(Base, "aes");


        public Models.FortniteAPI.Data GetAES()
        {
            var data = GetData(AES);
            return JsonConvert.DeserializeObject<AES>(data).Data;
        }

        public async Task<List<Cosmetic>> GetSaturnMisc()
        {
            return await AreItemsConverted(await AddExtraItems(new List<Cosmetic>(), ItemType.IT_Misc));
        }

        public async Task<List<Cosmetic>> RemoveItems(List<Cosmetic> items)
        {
            Logger.Log("Removing items");
            foreach (var section in _cloudStorageService.GetSections())
            {
                foreach (var key in section.Keys)
                {
                    try
                    {
                        var changes = _cloudStorageService.DecodeChanges(key.Value);

                        if (changes.removeItem)
                            items.RemoveAll(x => x.Id.ToLower() == changes.Item.ItemID.ToLower());

                        if (changes.RemoveOption)
                        {
                            var item = items.Find(x => x.Id == changes.Item.ItemID);
                            if (item == null || item == new Cosmetic()) continue;

                            item.CosmeticOptions.RemoveAll(x => changes.RemoveOptions.Contains(x.ItemDefinition));
                        }

                    }
                    catch (Exception e)
                    {
                        Logger.Log(e.ToString());
                    }
                }
            }

            return items;
        }

        public async Task<List<Cosmetic>> AddExtraItems(List<Cosmetic> items, ItemType itemType)
        {
            Logger.Log("Adding extra items");

            List<Cosmetic> extraItems = new List<Cosmetic>();

            foreach (var section in _cloudStorageService.GetSections())
            {
                foreach (var key in section.Keys)
                {
                    var changes = _cloudStorageService.DecodeChanges(key.Value);

                    if (changes.addOptions && changes.Item.ItemType == itemType)
                    {
                        var itemInList = items.FirstOrDefault(x => x.Id.ToLower() == changes.Item.ItemID.ToLower());

                        foreach (var option in changes.SwapOptions)
                        {
                            itemInList.CosmeticOptions.Add(new SaturnItem()
                            {
                                Name = option.ItemName,
                                Description = option.ItemDescription,
                                Icon = option.ItemIcon,
                                ItemDefinition = option.ItemID,
                                Rarity = option.Rarity.Value,
                                Options = new List<SaturnOption>()
                                {
                                    new SaturnOption()
                                    {
                                        Name = option.ItemName,
                                        Icon = option.ItemIcon,
                                        Rarity = option.Rarity.Value,
                                        Assets = option.OverrideAssets
                                    }
                                }
                            });
                        }

                        items.RemoveAll(x => x.Id == itemInList.Id);
                        items.Reverse();
                        items.Add(itemInList);
                        items.Reverse();
                    }

                    if (changes.addItem && changes.Item.ItemType == itemType)
                    {
                        List<SaturnItem> itemOptions = new();
                        for (int i = 0; i < changes.SwapOptions.Count; i++)
                        {
                            itemOptions.Add(new()
                            {
                                Name = changes.SwapOptions[i].ItemName,
                                Icon = changes.SwapOptions[i].ItemIcon,
                                Description = changes.SwapOptions[i].ItemDescription,
                                Rarity = changes.SwapOptions[i].Rarity.Value,
                                Options = new List<SaturnOption>()
                                {
                                    new SaturnOption()
                                    {
                                        Name = changes.SwapOptions[i].ItemName,
                                        Icon = changes.SwapOptions[i].ItemIcon,
                                        Rarity = changes.SwapOptions[i].Rarity.Value,
                                        Assets = changes.SwapOptions[i].OverrideAssets
                                    }
                                }
                            });
                        }


                        extraItems.Add(new Cosmetic()
                        {
                            Images = new() { SmallIcon = changes.Item.ItemIcon },
                            Description = changes.Item.ItemDescription,
                            Id = changes.Item.ItemID,
                            Name = changes.Item.ItemName,
                            Series = changes.Item.Series ?? new Series(),
                            Rarity = changes.Item.Rarity,
                            IsCloudAdded = true,
                            CosmeticOptions = itemOptions
                        });
                    }
                }
            }

            items.Reverse();
            items.AddRange(extraItems);
            items.Reverse();
            return items;
        }

        private async Task<List<Cosmetic>> IsHatTypeDifferent(List<Cosmetic> skins)
        {
            Logger.Log("Getting hat types");
            var DifferentHatsStr = _cloudStorageService.GetChanges("Skins", "HatTypes");

            Logger.Log(DifferentHatsStr);

            Logger.Log("Decoding hat types");
            var DifferentHats = _cloudStorageService.DecodeChanges(DifferentHatsStr).MiscData;

            foreach (var skin in skins)
            {
                skin.CosmeticOptions = new()
                {
                    new SaturnItem
                    {
                        ItemDefinition = "CID_970_Athena_Commando_F_RenegadeRaiderHoliday",
                        Name = "Gingerbread Raider",
                        Description = "Let the festivities begin.",
                        Icon = "https://fortnite-api.com/images/cosmetics/br/cid_970_athena_commando_f_renegaderaiderholiday/smallicon.png",
                        Rarity = "Rare"
                    },
                    new SaturnItem
                    {
                        ItemDefinition = "CID_784_Athena_Commando_F_RenegadeRaiderFire",
                        Name = "Blaze",
                        Description = "Fill the world with flames.",
                        Icon = "https://fortnite-api.com/images/cosmetics/br/cid_784_athena_commando_f_renegaderaiderfire/smallicon.png",
                        Rarity = "Legendary"
                    },
                    new SaturnItem
                    {
                        ItemDefinition = "CID_A_322_Athena_Commando_F_RenegadeRaiderIce",
                        Name = "Permafrost Raider",
                        Description = "What could freeze a heart that burns?",
                        Icon = "https://fortnite-api.com/images/cosmetics/br/cid_a_322_athena_commando_f_renegaderaiderice/smallicon.png",
                        Rarity = "Epic",
                        Series = "FrozenSeries"
                    },
                    new SaturnItem
                    {
                        ItemDefinition = "CID_294_Athena_Commando_F_RedKnightWinter",
                        Name = "Frozen Red Knight",
                        Description = "Frozen menace of icy tundra.",
                        Icon =
                            "https://fortnite-api.com/images/cosmetics/br/cid_294_athena_commando_f_redknightwinter/smallicon.png",
                        Rarity = "Legendary",
                        Series = "FrozenSeries"
                    },
                    new SaturnItem
                    {
                        ItemDefinition = "CID_653_Athena_Commando_F_UglySweaterFrozen",
                        Name = "Frozen Nog Ops",
                        Description = "Bring some chill to the skirmish.",
                        Icon = "https://fortnite-api.com/images/cosmetics/br/cid_653_athena_commando_f_uglysweaterfrozen/smallicon.png",
                        Rarity = "Epic",
                        Series = "FrozenSeries"
                    },
                    new SaturnItem
                    {
                        ItemDefinition = "CID_A_311_Athena_Commando_F_ScholarFestiveWinter",
                        Name = "Blizzabelle",
                        Description = "Voted Teen Queen of Winterfest by a jury of her witchy peers.",
                        Icon = "https://fortnite-api.com/images/cosmetics/br/cid_a_311_athena_commando_f_scholarfestivewinter/smallicon.png",
                        Rarity = "Rare"
                    },
                    new SaturnItem
                    {
                        ItemDefinition = "CID_A_007_Athena_Commando_F_StreetFashionEclipse",
                        Name = "Ruby Shadows",
                        Description = "Sometimes you gotta go dark.",
                        Icon = "https://fortnite-api.com/images/cosmetics/br/cid_a_007_athena_commando_f_streetfashioneclipse/smallicon.png",
                        Rarity = "Epic",
                        Series = "ShadowSeries"
                    },
                    new SaturnItem
                    {
                        ItemDefinition = "CID_936_Athena_Commando_F_RaiderSilver",
                        Name = "Diamond Diva",
                        Description = "Synthetic diamonds need not apply.",
                        Icon = "https://fortnite-api.com/images/cosmetics/br/cid_936_athena_commando_f_raidersilver/smallicon.png",
                        Rarity = "Rare"
                    }
                };
                if (string.IsNullOrEmpty(skin.VariantChannel))
                    skin.CosmeticOptions.Add(new SaturnItem
                    {
                        ItemDefinition = "CID_A_275_Athena_Commando_F_Prime_D",
                        Name = "Default",
                        Description = "Standard issue Island combatant.",
                        Icon =
                            "https://fortnite-api.com/images/cosmetics/br/cid_a_275_athena_commando_f_prime_d/smallicon.png",
                        Rarity = "Common"
                    });

                if (skin.IsRandom)
                {
                    skin.CosmeticOptions = new()
                    {
                        new SaturnItem
                        {
                            ItemDefinition = "CID_A_311_Athena_Commando_F_ScholarFestiveWinter",
                            Name = "Blizzabelle",
                            Description = "Voted Teen Queen of Winterfest by a jury of her witchy peers.",
                            Icon = "https://fortnite-api.com/images/cosmetics/br/cid_a_311_athena_commando_f_scholarfestivewinter/smallicon.png",
                            Rarity = "Rare"
                        },
                        new SaturnItem
                        {
                            ItemDefinition = "CID_A_007_Athena_Commando_F_StreetFashionEclipse",
                            Name = "Ruby Shadows",
                            Description = "Sometimes you gotta go dark.",
                            Icon = "https://fortnite-api.com/images/cosmetics/br/cid_a_007_athena_commando_f_streetfashioneclipse/smallicon.png",
                            Rarity = "Epic",
                            Series = "ShadowSeries"
                        },
                        new SaturnItem
                        {
                            ItemDefinition = "CID_936_Athena_Commando_F_RaiderSilver",
                            Name = "Diamond Diva",
                            Description = "Synthetic diamonds need not apply.",
                            Icon = "https://fortnite-api.com/images/cosmetics/br/cid_936_athena_commando_f_raidersilver/smallicon.png",
                            Rarity = "Rare"
                        },
                        new SaturnItem
                        {
                            ItemDefinition = "CID_784_Athena_Commando_F_RenegadeRaiderFire",
                            Name = "Blaze",
                            Description = "Fill the world with flames.",
                            Icon = "https://fortnite-api.com/images/cosmetics/br/cid_784_athena_commando_f_renegaderaiderfire/smallicon.png",
                            Rarity = "Legendary"
                        },
                        new SaturnItem
                        {
                            ItemDefinition = "CID_162_Athena_Commando_F_StreetRacer",
                            Name = "Redline",
                            Description = "Revving beyond the limit.",
                            Icon =
                                "https://fortnite-api.com/images/cosmetics/br/cid_162_athena_commando_f_streetracer/smallicon.png",
                            Rarity = "Epic"
                        },
                        new SaturnItem
                        {
                            ItemDefinition = "CID_294_Athena_Commando_F_RedKnightWinter",
                            Name = "Frozen Red Knight",
                            Description = "Frozen menace of icy tundra.",
                            Icon =
                                "https://fortnite-api.com/images/cosmetics/br/cid_294_athena_commando_f_redknightwinter/smallicon.png",
                            Rarity = "Legendary",
                            Series = "FrozenSeries"
                        }
                    };
                }

                if (DifferentHats.IndexOf(skin.Id) != -1)
                {
                    skin.HatTypes = HatTypes.HT_Hat;
                    skin.CosmeticOptions = new List<SaturnItem>()
                    {
                        new SaturnItem
                        {
                            ItemDefinition = "CID_162_Athena_Commando_F_StreetRacer",
                            Name = "Redline",
                            Description = "Revving beyond the limit.",
                            Icon =
                                "https://fortnite-api.com/images/cosmetics/br/cid_162_athena_commando_f_streetracer/smallicon.png",
                            Rarity = "Epic"
                        }
                    };
                    if (string.IsNullOrEmpty(skin.VariantChannel))
                        skin.CosmeticOptions.Add(new SaturnItem
                        {
                            ItemDefinition = "CID_A_275_Athena_Commando_F_Prime_D",
                            Name = "Default",
                            Description = "Standard issue Island combatant.",
                            Icon =
                                "https://fortnite-api.com/images/cosmetics/br/cid_a_275_athena_commando_f_prime_d/smallicon.png",
                            Rarity = "Common"
                        });
                }
            }

            return skins;
        }

        public async Task<List<Cosmetic>> AreItemsConverted(List<Cosmetic> items)
        {
            var ret = items;

            Logger.Log("Checking if items are converted...");
            var convertedItems = await _configService.TryGetConvertedItems();
            Logger.Log("Cross checking a converted items list of " + convertedItems.Count + " items...");

            if (convertedItems.Count > 0)
                convertedItems.Any(x => ret.Any(y =>
                {
                    if (y.Id != x.ItemDefinition) return false;
                    if (y.Name != x.Name) return false;
                    y.IsConverted = true;
                    return true;
                }));
            else
                Logger.Log("No converted items found.");

            return ret;
        }

        private string GetData(Uri uri)
        {
            using var wc = new WebClient();
            return wc.DownloadString(uri);
        }

        private async Task<string> GetDataAsync(Uri uri)
        {
            using var wc = new WebClient();
            return await wc.DownloadStringTaskAsync(uri);
        }

        private Uri CosmeticsByType(string type)
        {
            return new(Base, $"cosmetics/br/search/all?backendType={type}");
        }
    }
}