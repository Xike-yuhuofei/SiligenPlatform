#include "CommandHandlers.h"
#include "CommandHandlersInternal.h"

#include <utility>

namespace Siligen::Adapters::CLI {

using Siligen::Application::CommandLineConfig;
using Siligen::Application::CommandType;
using Siligen::Application::Container::ApplicationContainer;

CLICommandHandlers::CLICommandHandlers(std::shared_ptr<ApplicationContainer> container)
    : container_(std::move(container)) {}

int CLICommandHandlers::Execute(const CommandLineConfig& config) {
    switch (config.command) {
        case CommandType::CONNECT:
            return HandleConnect(config);
        case CommandType::DISCONNECT:
            return HandleDisconnect(config);
        case CommandType::STATUS:
            return HandleStatus(config);
        case CommandType::HOME:
            return HandleHome(config);
        case CommandType::JOG:
            return HandleJog(config);
        case CommandType::MOVE:
            return HandleMove(config);
        case CommandType::STOP_ALL:
            return HandleStopAll(config);
        case CommandType::EMERGENCY_STOP:
            return HandleEmergencyStop(config);
        case CommandType::DISPENSER_START:
        case CommandType::DISPENSER_PURGE:
        case CommandType::DISPENSER_STOP:
        case CommandType::DISPENSER_PAUSE:
        case CommandType::DISPENSER_RESUME:
            return HandleDispenser(config);
        case CommandType::SUPPLY_OPEN:
        case CommandType::SUPPLY_CLOSE:
            return HandleSupply(config);
        case CommandType::DXF_PLAN:
            return HandleDXFPlan(config);
        case CommandType::DXF_DISPENSE:
            return HandleDXFDispense(config);
        case CommandType::DXF_AUGMENT:
            return HandleDXFAugment(config);
        case CommandType::RECIPE_CREATE:
            return HandleRecipeCreate(config);
        case CommandType::RECIPE_UPDATE:
            return HandleRecipeUpdate(config);
        case CommandType::RECIPE_DRAFT_CREATE:
            return HandleRecipeDraftCreate(config);
        case CommandType::RECIPE_DRAFT_UPDATE:
            return HandleRecipeDraftUpdate(config);
        case CommandType::RECIPE_PUBLISH:
            return HandleRecipePublish(config);
        case CommandType::RECIPE_LIST:
            return HandleRecipeList(config);
        case CommandType::RECIPE_GET:
            return HandleRecipeGet(config);
        case CommandType::RECIPE_LIST_VERSIONS:
            return HandleRecipeListVersions(config);
        case CommandType::RECIPE_ARCHIVE:
            return HandleRecipeArchive(config);
        case CommandType::RECIPE_VERSION_CREATE:
            return HandleRecipeVersionCreate(config);
        case CommandType::RECIPE_VERSION_COMPARE:
            return HandleRecipeCompare(config);
        case CommandType::RECIPE_VERSION_ACTIVATE:
            return HandleRecipeActivate(config);
        case CommandType::RECIPE_AUDIT:
            return HandleRecipeAudit(config);
        case CommandType::RECIPE_EXPORT:
            return HandleRecipeExport(config);
        case CommandType::RECIPE_IMPORT:
            return HandleRecipeImport(config);
        case CommandType::INTERACTIVE:
        case CommandType::NONE:
        case CommandType::BOOTSTRAP_CHECK:
        default:
            std::cout << "未指定有效命令，使用交互模式" << std::endl;
            return RunInteractive();
    }
}

int CLICommandHandlers::RunInteractive() {
    CommandLineConfig connect_config;
    auto connect_result = ConnectHardware(connect_config, true);
    ShowConnectionResult(connect_result);

    const std::string default_dxf_path = LoadDefaultDxfFilePath(connect_config);

    while (true) {
        PrintHeader();
        std::cout << "请选择操作:\n"
                  << "  1. 重新连接（默认参数）\n"
                  << "  2. 断开连接\n"
                  << "  3. 系统状态查看\n"
                  << "  4. 轴回零\n"
                  << "  5. 点动/步进\n"
                  << "  6. 点位移动\n"
                  << "  7. 点胶阀控制\n"
                  << "  8. 排胶\n"
                  << "  9. 供胶阀控制\n"
                  << " 10. DXF路径规划\n"
                  << " 11. DXF点胶执行\n"
                  << " 12. DXF轮廓增强\n"
                  << " 13. 急停\n"
                  << " 14. 停止所有轴\n"
                  << "  0. 退出\n"
                  << "\n请输入选项: ";

        std::string input;
        if (!ReadLine(input)) {
            return 0;
        }
        input = StringManipulator::Trim(input);

        int choice = -1;
        if (!ParseInt(input, choice)) {
            std::cout << "无效输入，请重新选择" << std::endl;
            Pause();
            continue;
        }

        if (choice == 0) {
            std::cout << "退出程序" << std::endl;
            return 0;
        }

        CommandLineConfig cmd;
        cmd.dxf_file_path = default_dxf_path;

        switch (choice) {
            case 1:
                cmd.command = CommandType::CONNECT;
                cmd.command_string = "connect";
                cmd.auto_connect = true;
                HandleConnect(cmd);
                break;
            case 2:
                cmd.command = CommandType::DISCONNECT;
                cmd.command_string = "disconnect";
                HandleDisconnect(cmd);
                break;
            case 3:
                cmd.command = CommandType::STATUS;
                cmd.command_string = "status";
                cmd.show_motion = true;
                cmd.show_io = true;
                cmd.show_valves = true;
                HandleStatus(cmd);
                break;
            case 4:
                cmd.command = CommandType::HOME;
                cmd.command_string = "home";
                cmd.home_all_axes = true;
                HandleHome(cmd);
                break;
            case 5:
                cmd.command = CommandType::JOG;
                cmd.command_string = "jog";
                cmd.axis = static_cast<short>(PromptInt("轴号(1-2)", 1));
                cmd.direction = PromptYesNo("方向是否为正向", true) ? 1 : -1;
                cmd.distance = PromptDouble("步进距离(mm, 0.01-100)", 10.0);
                cmd.velocity = PromptDouble("速度(mm/s)", 0.0);
                HandleJog(cmd);
                break;
            case 6:
                cmd.command = CommandType::MOVE;
                cmd.command_string = "move";
                cmd.axis = static_cast<short>(PromptInt("轴号(1-2)", 1));
                cmd.position = PromptDouble("目标位置(mm)", 0.0);
                cmd.relative_move = PromptYesNo("相对运动", false);
                cmd.velocity = PromptDouble("速度(mm/s)", 0.0);
                cmd.acceleration = PromptDouble("加速度(mm/s^2)", 0.0);
                HandleMove(cmd);
                break;
            case 7: {
                std::cout << "点胶阀操作: 1)启动 2)暂停 3)恢复 4)停止\n";
                const int action = PromptInt("选择操作", 1);
                if (action == 1) {
                    cmd.command = CommandType::DISPENSER_START;
                    cmd.command_string = "dispenser start";
                    cmd.dispenser_count = static_cast<uint32>(PromptInt("点胶次数", static_cast<int>(cmd.dispenser_count)));
                    cmd.dispenser_interval_ms =
                        static_cast<uint32>(PromptInt("点胶间隔(ms)", static_cast<int>(cmd.dispenser_interval_ms)));
                    cmd.dispenser_duration_ms =
                        static_cast<uint32>(PromptInt("点胶时长(ms)", static_cast<int>(cmd.dispenser_duration_ms)));
                } else if (action == 2) {
                    cmd.command = CommandType::DISPENSER_PAUSE;
                    cmd.command_string = "dispenser pause";
                } else if (action == 3) {
                    cmd.command = CommandType::DISPENSER_RESUME;
                    cmd.command_string = "dispenser resume";
                } else {
                    cmd.command = CommandType::DISPENSER_STOP;
                    cmd.command_string = "dispenser stop";
                }
                HandleDispenser(cmd);
                break;
            }
            case 8:
                cmd.command = CommandType::DISPENSER_PURGE;
                cmd.command_string = "dispenser purge";
                cmd.wait_purge_key = PromptYesNo("等待排胶键触发", false);
                if (!cmd.wait_purge_key) {
                    cmd.wait_for_completion = PromptYesNo("达到时长后自动停止", true);
                }
                if (cmd.wait_purge_key || cmd.wait_for_completion) {
                    cmd.timeout_ms = PromptInt("最长等待或排胶时长(ms)", cmd.timeout_ms);
                }
                HandleDispenser(cmd);
                break;
            case 9: {
                std::cout << "供胶阀操作: 1)打开 2)关闭\n";
                const int action = PromptInt("选择操作", 1);
                cmd.command = (action == 1) ? CommandType::SUPPLY_OPEN : CommandType::SUPPLY_CLOSE;
                cmd.command_string = (action == 1) ? "supply open" : "supply close";
                HandleSupply(cmd);
                break;
            }
            case 10:
                cmd.command = CommandType::DXF_PLAN;
                cmd.command_string = "dxf-plan";
                cmd.dxf_file_path = PromptString("DXF文件路径", cmd.dxf_file_path);
                HandleDXFPlan(cmd);
                break;
            case 11:
                cmd.command = CommandType::DXF_DISPENSE;
                cmd.command_string = "dxf-dispense";
                cmd.dxf_file_path = PromptString("DXF文件路径", cmd.dxf_file_path);
                cmd.auto_home_axes = PromptYesNo("检测到未回零是否自动回零", true);
                HandleDXFDispense(cmd);
                break;
            case 12:
                cmd.command = CommandType::DXF_AUGMENT;
                cmd.command_string = "dxf-augment";
                cmd.dxf_file_path = PromptString("DXF文件路径", cmd.dxf_file_path);
                cmd.dxf_output_path = PromptString("输出路径(可选)", "");
                HandleDXFAugment(cmd);
                break;
            case 13:
                cmd.command = CommandType::EMERGENCY_STOP;
                cmd.command_string = "estop";
                HandleEmergencyStop(cmd);
                break;
            case 14:
                cmd.command = CommandType::STOP_ALL;
                cmd.command_string = "stop-all";
                cmd.immediate_stop = PromptYesNo("立即停止", false);
                HandleStopAll(cmd);
                break;
            default:
                std::cout << "无效选项，请重新选择" << std::endl;
                break;
        }

        Pause();
    }
}

CommandLineConfig CLICommandHandlers::ApplyDxfDefaults(const CommandLineConfig& config) const {
    if (!config.dxf_file_path.empty()) {
        return config;
    }

    CommandLineConfig resolved = config;
    resolved.dxf_file_path = LoadDefaultDxfFilePath(config);
    return resolved;
}

std::string CLICommandHandlers::LoadDefaultDxfFilePath(const CommandLineConfig& config) const {
    std::string value;
    const auto config_path = ResolveCliConfigPath(config.config_path);
    if (ReadIniValue(config_path, "DXF", "dxf_file_path", value)) {
        return value;
    }

    std::cerr << "[WARNING] CLI: 未读取到配置项 [DXF] dxf_file_path，使用默认DXF路径: "
              << kFallbackDxfFilePath << std::endl;
    return kFallbackDxfFilePath;
}

void CLICommandHandlers::PrintHeader() const {
    ClearScreen();
    std::cout << "========================================" << std::endl;
    std::cout << "   Siligen 点胶机控制系统" << std::endl;
    std::cout << "   交互式菜单模式" << std::endl;
    std::cout << "========================================\n" << std::endl;
}

void CLICommandHandlers::Pause() const {
    std::cout << "\n按 Enter 键继续..." << std::endl;
    std::string input;
    ReadLine(input);
}

}  // namespace Siligen::Adapters::CLI
