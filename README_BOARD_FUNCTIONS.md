AD5941 DPV 监测板功能说明

1 当前固件状态

1.1 当前 sketch_apr5a.ino 是串口上位机版本。

1.2 固件不启动 WiFi，不启动网页，不初始化 TF 卡。

1.3 固件只做三件事：
1.3.1 通过 ESP32-C3 控制 AD5941。
1.3.2 执行 DPV 扫描。
1.3.3 通过串口把参数、状态和数据上传到电脑上位机。

1.4 默认串口速率是 115200 baud。115200 baud 在常见 8N1 串口格式下，约等于 115200 bit/s，有效数据吞吐约 11520 byte/s。DPV 实际点速通常由脉冲时间、稳定等待时间和扫描速率决定，一般远低于 100 点/s，所以当前默认速率足够稳妥。以后如果上位机和 USB 转串口模块都稳定，可以再提高到 230400 或 460800。

1.5 当前版本适合下一步制作电脑上位机。参数调整界面、开始停止按钮、实时曲线和 CSV 保存都建议放在上位机完成。

2 硬件引脚

2.1 最终确认的 ESP32-C3 引脚如下。

2.2 IO0：AD5941 GPIO0，中断输出。

2.3 IO4：AD5941 GPIO1，中断输出。

2.4 IO5：SPI SCLK。

2.5 IO6：SPI MOSI。

2.6 IO7：SPI MISO。

2.7 IO10：AD5941 片选。

2.8 IO18：TF 卡片选。当前串口版本不使用 TF 卡，但会保持 TF_CS 为高电平。

2.9 IO1：ADG704 MUL_EN。

2.10 IO3：ADG704 MUL_A0。

2.11 IO19：ADG704 MUL_A1。

2.12 IO2：当前保留未用。

3 串口连接建议

3.1 推荐供电和串口分离。

3.2 烧录模块连接电脑 Type-C，用于烧录和串口通信。

3.3 烧录模块连接电路板的 GND、TX、RX、EN、BOOT。

3.4 烧录模块 VCC 不接电路板。

3.5 电路板由自己的 5V 稳压电源或电池系统供电。

3.6 必须共地，也就是烧录模块 GND 和电路板 GND 必须连接。

3.7 这样可以避免电脑 USB 5V 和电池 5V 互相顶，也能减少 brownout 复位风险。

4 串口协议

4.1 串口使用换行作为命令结束。上位机发送一行命令，ESP32 解析一行。

4.2 常用命令如下。

4.3 HELP：打印命令帮助。

4.4 STATUS：打印当前运行状态。

4.5 PARAM?：打印当前全部参数。

4.6 ID?：重新读取 AD5941 身份寄存器。

4.7 START：开始一次 DPV 扫描。

4.8 STOP：停止当前扫描。

4.9 SET key value：设置参数。例如 SET start -200，SET end 600，SET pulse_ms 50。

4.10 支持的 key 如下。

4.11 we：工作电极通道，1 到 4。

4.12 start：起始电位，单位 mV。

4.13 end：结束电位，单位 mV。

4.14 step：步进电位，单位 mV。

4.15 pulse_amp：脉冲幅度，单位 mV。

4.16 scan_rate：扫描速率，单位 mV/s。

4.17 pulse_ms：脉冲时间，单位 ms。

4.18 quiet_ms：开始前静置时间，单位 ms。

4.19 settle_ms：base 电位采样前等待时间，单位 ms。

4.20 vzero：AD5941 LPDAC VZERO，单位 mV。

4.21 rcal：板载 RCAL 校准电阻值，单位 ohm。当前原理图中 RCAL 是右边 R6，连接在 RCAL0 和 RCAL1 之间，阻值为 200 ohm。左边 R5 10M 和 C17 220pF 是 RC0_0、RC0_1 的高阻电容网络，不是 RCAL。

4.22 rtia：跨阻电阻值，单位 ohm。

4.23 adc_ref：ADC reference，单位 V。

4.24 adc_pga：ADC PGA 增益。

4.25 timeout_ms：等待一次 FIFO 样本的最长时间，单位 ms。

4.26 max_points：本次扫描最大点数。

5 串口输出格式

5.1 以 # 开头的是状态、参数、错误或说明行。

5.2 数据行以 DATA 开头，格式如下。

5.3 DATA,t_ms,we_channel,segment,index,e_base_mV,e_pulse_mV,i_base_uA,i_pulse_uA,dI_uA,valid

5.4 valid 为 1 表示该点电流有效，valid 为 0 表示当前电极开路、前端饱和或换算结果为 nan/inf。

5.5 上位机保存 CSV 时，可以只保存 DATA 行，也可以把 #PARAM 和 #INFO 行写到 CSV 文件开头作为实验元数据。

6 当前为什么不使用 TF 卡

6.1 原始设想是使用 TF 卡保存 WiFi 账号密码，并在停止采集时把 CSV 保存到 TF 卡。

6.2 实测发现当前 MicroSD Card Adapter 模块和 AD5941 同时挂在同一组 SPI 时，会影响 AD5941 的 MISO 读取。

6.3 关键实验结果是：TF_CS 一直保持高电平，且固件完全不调用 SD/SdFat，只读取 AD5941 ID；当 TF 模块上电时 AD5941 读到 0xFFFFFFFF；拔掉 TF 模块 5V 后，AD5941 立即恢复 0x4144 和 0x5502。

6.4 因此当前问题不是软件流程，也不是 SD 库初始化顺序，而是 TF 模块上电后对共享 MISO 总线产生影响。

6.5 为避免寄到生化实验室后因为 TF 卡模块导致 AD5941 失效，当前版本禁用 TF 卡。

7 当前为什么不使用 WiFi 网页

7.1 原始设想是 ESP32 启动网页，手机通过局域网或 ESP32 热点查看曲线和设置参数。

7.2 该方案本身可行，但现场需要处理 WiFi 名称、密码、IP 地址、手机浏览器兼容性以及数据完整性。

7.3 当前为了降低寄出后的不确定性，先采用串口上位机方案。电脑上位机可以固定串口、固定保存路径、实时画图，并完整保存数据。

7.4 WiFi 网页可以作为后续功能恢复，但不作为当前寄出版本的主流程。

8 后续可增加的需求

8.1 上位机参数界面：显示并修改全部 DPV 参数，发送 SET 命令到设备。

8.2 上位机实时曲线：解析 DATA 行，实时绘制电位对 dI 的曲线。

8.3 上位机保存 CSV：保存参数、设备身份、开始时间、结束时间和所有 DATA 行。

8.4 上位机设备检查：启动时发送 ID?，确认 AD5941 为 0x4144，CHIPID 为 0x5502。

8.5 上位机异常提示：如果收到 SAMPLE_TIMEOUT、AD5941_NOT_READY、APP_AMP_INIT 等错误，给实验人员显示中文提示。

8.6 WiFi 网页恢复：未来如果需要手机显示，可恢复 ESP32 AP 模式，默认热点地址为 192.168.4.1。

8.7 TF 卡恢复：未来如果换成支持 SPI 多从机共享的 TF 模块，或把 TF MISO 单独接到其他 GPIO 并使用软件 SPI，可以重新加入 TF 保存。

8.8 电源检查：正式实验建议用电路板自己的 5V 稳压电源供电，烧录模块只接 GND、TX、RX、EN、BOOT，不接 VCC。

9 寄出前检查

9.1 不接 TF 卡模块，或确保 TF 模块不向共享 MISO 上电。

9.2 使用稳定 5V 给电路板供电。

9.3 烧录模块 VCC 不接电路板，GND 必须共地。

9.4 串口打开 115200 baud。

9.5 上电后应看到 #IDENTITY 行，并且 ok 为 1。

9.6 发送 PARAM?，确认参数正确。

9.7 发送 START，确认出现 #HEADER 和 DATA 行。

9.8 发送 STOP，确认设备停止并输出 BATCH_END。
11 本次固件健壮性更新

11.1 上位机截图中电位和序号正常增加，但电流全是 nan，这不是上位机解析错误，而是下位机换算电流时得到非有限数。
11.2 根因是 Amperometric 例程在内部 RTIA 场景下，后续初始化可能把 RTIA 校准值覆盖为 0，导致电流换算除以 0。
11.3 现在代码保留内部 RTIA 的校准结果；当校准值异常时，使用当前 rtia 参数选择的内部 RTIA 标称值作为兜底。
11.4 生化实验室使用时，如果仍然出现 valid 为 0，需要优先检查电极是否泡入溶液、RE/CE/WE 是否接对、工作电极通道是否选对、RTIA 和 ADC PGA 是否过大导致饱和。
12 ADG704BRMZ 分时复用 WE 的最终需求

12.1 本板子使用 ADG704BRMZ 把 AD5941 的 SE0 分时连接到多个工作电极 WE。ADG704 是 4 选 1 模拟开关，同一时刻只能导通一个 S 通道到公共端 D，也就是同一时刻只能采一个 WE。

12.2 ESP32-C3 与 ADG704BRMZ 的控制脚对应关系如下。

12.2.1 IO1 接 MUL_EN，也就是 ADG704 的 EN。
12.2.2 IO3 接 MUL_A0，也就是 ADG704 的 A0。
12.2.3 IO19 接 MUL_A1，也就是 ADG704 的 A1。

12.3 原理图中 ADG704BRMZ 的模拟通道对应关系如下。

12.3.1 S1 接 WE1。
12.3.2 S2 接 WE2。
12.3.3 S3 接 WE3。
12.3.4 S4 接 WE4，当前可以保留不用。
12.3.5 D 接 AD5941 的 SE0。

12.4 当前代码按照 ADG704 的二进制地址选择通道。

12.4.1 WE1 对应 A1=0，A0=0。
12.4.2 WE2 对应 A1=0，A0=1。
12.4.3 WE3 对应 A1=1，A0=0。
12.4.4 WE4 对应 A1=1，A0=1。
12.4.5 当前固件开放 WE1、WE2、WE3 给上位机选择，WE4 作为硬件预留通道，默认不参与批次扫描。

12.5 EN 的使用方式是先关闭模拟开关，再设置 A0 和 A1，等待 2 ms 后打开模拟开关，再等待 10 ms 让通道稳定。ADG704BRMZ 的 EN 为 1 时使能选中的通道，EN 为 0 时所有通道断开。这样可以减少切换瞬间把错误 WE 接到 SE0 的风险。

12.5.1 如果 EN 逻辑写反，WE 实际不会接到 SE0。现象是手捏电极会有一点漂移变化，但接 100 kΩ 假负载时电流不会按 0 到 2 uA 线性变化，而是只有 nA 级噪声。

12.6 新固件支持多 WE 分时 DPV 批次扫描。上位机可以选择 1 个、2 个或 3 个 WE，下位机收到 START 后自动按选择列表分时扫描，每个数据点都会上传 we_channel 和 segment。

12.7 当前扫描顺序是轮询式分时复用，不是一个 WE 扫完再切换。上位机选择 WE1、WE2、WE3 时，下位机在同一个 segment 内按点轮询，顺序是 WE1 point0、WE2 point0、WE3 point0、WE1 point1、WE2 point1、WE3 point1。这样上位机显示时看起来像多个 WE 同时进行。

12.7.1 如果 segment 为 2，扫描顺序是先完成第 1 个 segment 的 WE 轮询，再进入第 2 个 segment 的 WE 轮询。

12.7.2 每完成一个 WE 的一个点，下位机立刻上传一行 DATA，上位机收到后按 we_channel 和 segment 分组显示。

12.7.3 多 WE 轮询会增加单个 WE 相邻两个点之间的实际时间间隔。例如一个 WE 的 base 和 pulse 采样需要约 70 ms，三个 WE 轮询时，WE1 point0 到 WE1 point1 的实际间隔至少接近 210 ms，再加上初始化和串口开销。因此多 WE 模式的实际扫描速度会比单 WE 慢。上位机记录时应以 DATA 里的 t_ms 作为真实时间。

12.8 System Setup 属于全局参数，所有 WE 共用。包括 ADC Ref、ADC PGA、VZERO、RCAL、RTIA、Timeout、Max points。这样不同 WE 的电流换算系数一致，后续标定和比较更可靠。

12.9 DPV Setup 属于方法参数，允许每个 WE 单独设置。包括 start、end、step、pulse_amp、scan_rate、pulse_ms、quiet_ms、settle_ms、segments。这样 WE1、WE2、WE3 可以用于不同修饰电极或不同检测条件。

12.10 下位机仍然保留单 WE 兼容模式。只发送 SET we 1 和 START 时，行为等价于旧版本，只扫描 WE1 一轮或该 WE 当前设置的 segments。
13 固件防呆和上位机错误提示需求

13.1 固件现在不再静默接受错误参数。用户输入不是数字、参数超范围、WE 选择错误、START 前配置不合理时，下位机会通过串口返回 #ERROR,code,message。上位机必须监听并显示这些错误。

13.2 参数设置成功时返回 #OK,SET,key,value。上位机应该只有在收到 #OK 后，才把界面状态标记为已经写入下位机。

13.3 参数设置失败时返回 #ERROR。常见错误包括 PARAM_NOT_NUMBER、PARAM_RANGE、UNKNOWN_PARAM、WE_LIST_EMPTY、WE_LIST_RANGE。上位机应把 message 显示给用户，并让用户重新输入。

13.4 START 前固件会做配置检查。没有选择 WE、start 等于 end、max_points 不够、pulse 后电位超范围、AD5941 未准备好，都会拒绝启动并返回 #ERROR。

13.5 运行中如果 AD5941 初始化失败、FIFO 超时或 ID 读取失败，也会返回 #ERROR。上位机应停止当前批次，保留已经收到的数据，并提示用户检查供电、SPI、TF 模块、AD5941、溶液和电极连接。

13.6 如果 scan_rate 设置过快，固件返回 #WARN,SCAN_RATE_LIMITED。这个警告不一定终止实验，但表示实际扫描速度会低于用户设置。多 WE 轮询时，一个 AD5941 要依次服务 WE1、WE2、WE3，所以实际单通道点间隔会变长。

13.7 上位机开发者必须按照 README_DPV_PARAMETERS.md 第 13 节处理 #OK、#ERROR、#WARN、#PARAM、#WE_PARAM、#HEADER、DATA、#EVENT。不要只解析 DATA，否则用户参数打错时界面无法解释原因。

14 RTIA 来源选择的新需求

14.1 正式 DPV 测量只推荐使用内部 RTIA。上位机的 System Setup 正式测量界面应默认显示内部 RTIA，并提供 RTIA ohm 输入框。

14.2 外部 RTIA AIN1、AIN2、AIN3 不能作为正式 DPV 测量选项直接开放给实验人员。实测 100 kΩ 假负载时，内部 RTIA 能得到 100 mV 约 1 uA、200 mV 约 2 uA 的正确线性结果；外部 AIN2 25.5 kΩ 和 AIN1 470 kΩ 会出现固定偏置电流，不能按外部 100 kΩ 线性变化。

14.3 上位机可以保留一个单独的校准或硬件诊断页面，在这个页面里提供外部 RTIA 选择。这个页面必须明确写明：外部 RTIA 仅用于硬件诊断和校准验证，暂不用于正式 DPV 数据采集。

14.4 校准或硬件诊断页面可以提供 4 个 RTIA 来源选项。

14.4.1 内部 RTIA。

14.4.2 外部 RTIA AIN3 2 kΩ。

14.4.3 外部 RTIA AIN2 25.5 kΩ。

14.4.4 外部 RTIA AIN1 470 kΩ。

14.5 当用户选择内部 RTIA 时，上位机显示 RTIA ohm 输入框。上位机需要发送 SET rtia_source internal，然后再发送 SET rtia 数值。例如 SET rtia 10000。

14.6 当用户选择外部 RTIA AIN3 2 kΩ、AIN2 25.5 kΩ、AIN1 470 kΩ 时，上位机不要显示可编辑 RTIA ohm 输入框，因为这三个电阻是板子上固定焊接的电阻。上位机只发送 RTIA 来源命令即可。

14.6.1 外部 AIN3 2 kΩ 发送 SET rtia_source ext_ain3_2k。

14.6.2 外部 AIN2 25.5 kΩ 发送 SET rtia_source ext_ain2_25k5。

14.6.3 外部 AIN1 470 kΩ 发送 SET rtia_source ext_ain1_470k。

14.7 固件启动和 PARAM? 会打印 rtia_source、rtia_internal_ohm、rtia_effective_ohm、rtia_usage。上位机界面应显示 rtia_effective_ohm，作为当前参与电流换算的 RTIA。

14.8 rtia_usage 为 dpv_measurement 时，表示可以用于正式 DPV。rtia_usage 为 diagnostic_only 时，表示只用于硬件诊断，不要用于正式 DPV 数据。

14.9 固件在外部 RTIA 模式下收到 START 时，会输出警告 EXTERNAL_RTIA_DIAGNOSTIC_ONLY。上位机必须把这个警告显示给用户，或者在正式测量界面禁止 START。

14.10 内部 RTIA 模式仍然使用 AD5941 内部 LPTIA RTIA。固件会尝试官方校准；如果官方校准结果偏离内部 RTIA 标称值 0.5 倍到 2 倍范围之外，固件会自动丢弃异常校准值，改用内部 RTIA 标称值，避免电流比例严重错误。

14.11 外部 RTIA 模式会把内部 RTIA 打开，并把 ExtRtiaVal 设置为固定外部电阻值。外部模式不做内部 RTIA 校准，也不会被内部 RTIA 的标称兜底覆盖。但是当前实测结果说明这条外部路径不适合作为正式 DPV 的 RTIA。

14.12 外部 RTIA 是否真正能作为某种校准路径，需要用 100 kΩ 假负载逐项验证。验证时推荐 start 0 mV、end 200 mV、step 20 mV、pulse_amp 0 mV、settle_ms 100 ms、pulse_ms 50 ms、segments 1。

14.13 上位机要把 RTIA 来源写入 CSV 文件头或实验元数据。建议至少保存 rtia_source、rtia_internal_ohm、rtia_effective_ohm、rtia_usage、adc_ref、adc_pga、vzero、rcal。

14.14 当前版本仍然是上位机点击启动。用户在上位机设置好内部 RTIA、WE 通道和 DPV 参数后，上位机发送 START，固件才开始正式采样。外部 RTIA 测试应放在校准或硬件诊断流程中。
