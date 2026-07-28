// 在 NapCat WebUI 中暴露的配置项（plugin_config_ui）。
// 用户保存后，NapCat 会调用本插件的 plugin_set_config 写入这些值。
export const plugin_config_ui = [
  {
    key: "msm_control_url",
    label: "MSM 控制通道地址",
    type: "string",
    default: "http://127.0.0.1:25585",
    placeholder: "http://127.0.0.1:25585",
    description: "MSM 内置控制通道(25585)，独立于 WebUI，WebUI 关闭时仍可用",
  },
  {
    key: "msm_target_server",
    label: "默认服务器名",
    type: "string",
    default: "",
    placeholder: "留空=自动选运行中的",
    description: "省略服务器名时默认操作的服务器",
  },
  {
    key: "msm_allowed_commands",
    label: "指令白名单",
    type: "string",
    default:
      "op,deop,gamemode,tp,give,kick,ban,pardon,whitelist,time,weather,say,tell,msg,list,stop",
    description: "命令 转发允许的 MC 指令(小写,逗号分隔)",
  },
  {
    key: "msm_admins",
    label: "管理员 QQ",
    type: "string",
    default: "",
    placeholder: "123456,654321",
    description: "允许使用敏感指令的 QQ 号；留空=不限制",
  },
  {
    key: "msm_admin",
    label: "异常日志接收号",
    type: "string",
    default: "",
    placeholder: "123456",
    description: "服务器异常退出时日志私信到此 QQ",
  },
  {
    key: "msm_notify_targets",
    label: "占用推送目标",
    type: "string",
    default: "",
    placeholder: "group:群号,private:QQ号",
    description: "占用概览等 all 推送目标(逗号分隔)；留空关闭推送",
  },
];
