#include <ll/api/plugin/Plugin.h>
#include <ll/api/data/JsonConfig.h>
#include <ll/api/resource/PackLoader.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/entity/Player.h>
#include <nlohmann/json.hpp>
#include <string>
#include <algorithm>

// ===================== 常量定义（全程不可通过配置修改） =====================
constexpr int MAX_HEIGHT_LIMIT   = 512;   // 最高上边界硬上限
constexpr int HEIGHT_ALIGN_STEP  = 16;    // 游戏强制16格对齐

// 三个维度固定参数（最低点永远沿用原版，不对外暴露）
constexpr int OVERWORLD_MIN_Y    = -64;
constexpr int OVERWORLD_DEFAULT_MAX_Y = 320;

constexpr int NETHER_MIN_Y       = 0;
constexpr int NETHER_DEFAULT_MAX_Y    = 128;

constexpr int END_MIN_Y          = 0;
constexpr int END_DEFAULT_MAX_Y       = 256;

// ===================== 全局变量 =====================
// 正式生效的配置
int g_owFinalMaxY;
int g_netherFinalMaxY;
int g_endFinalMaxY;
std::string g_returnCommand;
bool g_enableDebugLog;

// GUI编辑临时配置（未保存前不生效）
int g_tempOwMax;
int g_tempNetherMax;
int g_tempEndMax;
std::string g_tempReturnCmd;
bool g_tempDebugLog;

// ===================== 核心高度校验函数 =====================
int validateMaxHeight(int inputMaxY, int defaultMaxY) {
    int aligned = (inputMaxY + HEIGHT_ALIGN_STEP / 2) / HEIGHT_ALIGN_STEP * HEIGHT_ALIGN_STEP;
    aligned = std::max(aligned, defaultMaxY);
    aligned = std::min(aligned, MAX_HEIGHT_LIMIT);
    return aligned;
}

// ===================== 维度配置生成（兼容1.21+全版本） =====================
nlohmann::json buildBaseDimensionType(int minY, int maxY, bool isOverworld) {
    int totalHeight = maxY - minY;
    return {
        {"min_y", minY},
        {"height", totalHeight},
        {"logical_height", totalHeight},
        {"coordinate_scale", 1.0},
        {"ambient_light", isOverworld ? 1.0 : 0.0},
        {"natural", isOverworld},
        {"has_skylight", isOverworld},
        {"has_ceiling", false},
        {"ultrawarm", false},
        {"bed_works", isOverworld},
        {"respawn_anchor_works", !isOverworld},
        {"has_raids", isOverworld},
        {"min_supported_y", minY},
        {"max_supported_y", maxY}
    };
}

// ===================== 配置加载与校验 =====================
bool reloadConfig() {
    ll::data::JsonConfig config("plugins/CustomDimensionHeight/config.json");
    
    // 读取正式配置
    int rawOwMax     = config["overworld_max_y"].get<int>();
    int rawNetherMax = config["nether_max_y"].get<int>();
    int rawEndMax    = config["end_max_y"].get<int>();
    g_returnCommand  = config["return_command"].get<std::string>();
    g_enableDebugLog = config["enable_debug_log"].get<bool>();
    
    // 执行完整校验
    g_owFinalMaxY     = validateMaxHeight(rawOwMax, OVERWORLD_DEFAULT_MAX_Y);
    g_netherFinalMaxY = validateMaxHeight(rawNetherMax, NETHER_DEFAULT_MAX_Y);
    g_endFinalMaxY    = validateMaxHeight(rawEndMax, END_DEFAULT_MAX_Y);
    
    // 回写修正后的值
    config["overworld_max_y"] = g_owFinalMaxY;
    config["nether_max_y"]    = g_netherFinalMaxY;
    config["end_max_y"]       = g_endFinalMaxY;
    config.save();
    
    // 同步初始化临时变量
    g_tempOwMax     = g_owFinalMaxY;
    g_tempNetherMax = g_netherFinalMaxY;
    g_tempEndMax    = g_endFinalMaxY;
    g_tempReturnCmd = g_returnCommand;
    g_tempDebugLog  = g_enableDebugLog;
    
    return true;
}

// ===================== GUI 界面函数声明 =====================
void openMainMenu(ll::entity::Player* player);
void openDimensionSetting(ll::entity::Player* player, int dimType);
void openMenuSetting(ll::entity::Player* player);
void saveTempConfig(ll::entity::Player* player);
void executeReturn(ll::entity::Player* player);

// ===================== 主菜单 GUI =====================
void openMainMenu(ll::entity::Player* player) {
    ll::form::SimpleForm form(
        "CustomDimensionHeight 管理面板",
        "修改后请点击「保存并重载配置」生效，维度高度需重启服务器后完全生效\n"
        "规则：16倍数自动对齐 | 最低不低于原版 | 最高不超过512"
    );

    form.appendButton("主世界高度设置", [player](auto) {
        openDimensionSetting(player, 0);
    });
    form.appendButton("下界高度设置", [player](auto) {
        openDimensionSetting(player, 1);
    });
    form.appendButton("末地高度设置", [player](auto) {
        openDimensionSetting(player, 2);
    });
    form.appendButton("菜单管理", [player](auto) {
        openMenuSetting(player);
    });
    form.appendButton("保存并重载配置", [player](auto) {
        saveTempConfig(player);
    });
    form.appendButton("返回", [player](auto) {
        executeReturn(player);
    });

    form.sendTo(player);
}

// ===================== 维度设置子界面 =====================
void openDimensionSetting(ll::entity::Player* player, int dimType) {
    std::string title;
    int currentValue;
    int defaultValue;

    switch (dimType) {
        case 0:
            title = "主世界高度设置";
            currentValue = g_tempOwMax;
            defaultValue = OVERWORLD_DEFAULT_MAX_Y;
            break;
        case 1:
            title = "下界高度设置";
            currentValue = g_tempNetherMax;
            defaultValue = NETHER_DEFAULT_MAX_Y;
            break;
        case 2:
            title = "末地高度设置";
            currentValue = g_tempEndMax;
            defaultValue = END_DEFAULT_MAX_Y;
            break;
        default:
            openMainMenu(player);
            return;
    }

    ll::form::CustomForm form(title);
    form.appendLabel("当前值：" + std::to_string(currentValue) + " | 原版默认：" + std::to_string(defaultValue) + " | 上限：512");
    form.appendInput("height", "请输入新的上边界高度", std::to_string(currentValue));

    form.sendTo(player, [player, dimType](auto result) {
        if (result.empty()) {
            openMainMenu(player);
            return;
        }

        try {
            int inputVal = std::stoi(result["height"].get<std::string>());
            switch (dimType) {
                case 0: g_tempOwMax = inputVal; break;
                case 1: g_tempNetherMax = inputVal; break;
                case 2: g_tempEndMax = inputVal; break;
            }
        } catch (...) {}
        openMainMenu(player);
    });
}

// ===================== 菜单管理界面（含调试日志开关） =====================
void openMenuSetting(ll::entity::Player* player) {
    ll::form::CustomForm form("菜单管理");
    form.appendLabel("设置主界面「返回」按钮执行的指令，例如 menu cd 等");
    form.appendInput("cmd", "返回指令（无需加/）", g_tempReturnCmd);
    form.appendToggle("debug", "开启调试日志提示", g_tempDebugLog);

    form.sendTo(player, [player](auto result) {
        if (result.empty()) {
            openMainMenu(player);
            return;
        }
        g_tempReturnCmd = result["cmd"].get<std::string>();
        g_tempDebugLog = result["debug"].get<bool>();
        openMainMenu(player);
    });
}

// ===================== 保存配置并重载 =====================
void saveTempConfig(ll::entity::Player* player) {
    // 对临时值执行完整校验
    int finalOw     = validateMaxHeight(g_tempOwMax, OVERWORLD_DEFAULT_MAX_Y);
    int finalNether = validateMaxHeight(g_tempNetherMax, NETHER_DEFAULT_MAX_Y);
    int finalEnd    = validateMaxHeight(g_tempEndMax, END_DEFAULT_MAX_Y);

    // 写入配置文件
    ll::data::JsonConfig config("plugins/CustomDimensionHeight/config.json");
    config["overworld_max_y"]  = finalOw;
    config["nether_max_y"]     = finalNether;
    config["end_max_y"]        = finalEnd;
    config["return_command"]   = g_tempReturnCmd;
    config["enable_debug_log"] = g_tempDebugLog;
    config.save();

    // 同步到正式生效变量
    g_owFinalMaxY     = finalOw;
    g_netherFinalMaxY = finalNether;
    g_endFinalMaxY    = finalEnd;
    g_returnCommand   = g_tempReturnCmd;
    g_enableDebugLog  = g_tempDebugLog;

    // 同步临时变量为修正后的值
    g_tempOwMax     = finalOw;
    g_tempNetherMax = finalNether;
    g_tempEndMax    = finalEnd;

    // 根据开关决定是否输出详细提示
    if (g_enableDebugLog) {
        player->sendMessage("§a[CustomDimensionHeight] 配置已保存并重载");
        player->sendMessage("§e生效值：主世界" + std::to_string(finalOw) + 
                            " | 下界" + std::to_string(finalNether) + 
                            " | 末地" + std::to_string(finalEnd));
        player->sendMessage("§c注意：维度高度需重启服务器后完全生效");
    }

    openMainMenu(player);
}

// ===================== 返回上级菜单 =====================
void executeReturn(ll::entity::Player* player) {
    // 丢弃所有未保存修改，重置临时变量为正式值
    g_tempOwMax     = g_owFinalMaxY;
    g_tempNetherMax = g_netherFinalMaxY;
    g_tempEndMax    = g_endFinalMaxY;
    g_tempReturnCmd = g_returnCommand;
    g_tempDebugLog  = g_enableDebugLog;

    // 执行返回指令
    if (!g_returnCommand.empty()) {
        player->runCommand(g_returnCommand);
    }
}

// ===================== 插件主入口 =====================
extern "C" __declspec(dllexport) bool ll_plugin_load(ll::plugin::Plugin& plugin) {
    // 启动时执行首次配置校验
    reloadConfig();

    // 生成三个维度的基础配置
    auto overworldType = buildBaseDimensionType(OVERWORLD_MIN_Y, g_owFinalMaxY, true);
    
    auto netherType    = buildBaseDimensionType(NETHER_MIN_Y, g_netherFinalMaxY, false);
    netherType["ultrawarm"]              = true;
    netherType["has_ceiling"]            = true;
    netherType["respawn_anchor_works"]   = true;
    netherType["infiniburn"]             = "minecraft:infiniburn_nether";
    
    auto endType       = buildBaseDimensionType(END_MIN_Y, g_endFinalMaxY, false);
    endType["respawn_anchor_works"]      = false;
    endType["infiniburn"]                = "minecraft:infiniburn_end";

    // 注册内置数据包
    ll::resource::registerBuiltinResourcePack("CustomDimensionHeight_Pack", [&](auto& builder) {
        builder.addJson("data/minecraft/dimension_type/overworld.json", overworldType);
        builder.addJson("data/minecraft/dimension_type/the_nether.json", netherType);
        builder.addJson("data/minecraft/dimension_type/the_end.json", endType);
    });

    // 注册管理员命令
    ll::command::CommandRegistrar::getInstance().registerCommand(
        "cdh",
        "CustomDimensionHeight 维度高度管理",
        CommandPermissionLevel::GameMasters,
        [&](CommandOrigin const& origin, CommandOutput& output) {
            // 游戏内玩家执行：打开GUI
            if (origin.getPlayer()) {
                openMainMenu(origin.getPlayer());
                return;
            }
            // 控制台执行：输出文字版信息
            output.success("CustomDimensionHeight 配置已加载");
            if (g_enableDebugLog) {
                output.success("主世界：%d | 下界：%d | 末地：%d", 
                    g_owFinalMaxY, g_netherFinalMaxY, g_endFinalMaxY);
            }
            output.success("GUI管理请在游戏内使用 /cdh 命令");
        }
    );

    return true;
}