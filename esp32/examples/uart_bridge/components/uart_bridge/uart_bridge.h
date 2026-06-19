#pragma once

/*
 * K230 ↔ ESP32 UART 桥（传输层）。
 * 收发都是 NDJSON：一行一条 JSON，以 '\n' 结尾。
 * 本组件只负责"搬运整行"，不解析 JSON —— 解析交给上层（用 cJSON）。
 * 协议见 shared/protocol/protocol.md。引脚见 bsp_board.h（UART1）。
 */

/**
 * @brief 收到一行完整 JSON（不含结尾 '\n'）时回调。
 *        在内部 RX 任务上下文里调用，别在里面做阻塞/重活。
 */
typedef void (*uart_bridge_line_cb_t)(const char *line);

/**
 * @brief 初始化 UART1（115200 8N1，BSP 引脚）。
 */
void uart_bridge_init(void);

/**
 * @brief 启动接收任务，按 '\n' 切行，每整行回调一次 cb。
 * @param cb  行回调（可为 NULL，表示只发不收）
 */
void uart_bridge_start_rx(uart_bridge_line_cb_t cb);

/**
 * @brief 发送一条 JSON。内部自动补 '\n'。
 * @param json  一行 JSON 字符串（不用自己加换行）
 */
void uart_bridge_send_json(const char *json);
