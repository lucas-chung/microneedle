AD5941 DPV 参数与标定说明

1 当前测量方法

1.1 当前固件实现 DPV，也就是差分脉冲伏安法。

1.2 每个电位点先在 base potential 采样，得到 I_base。

1.3 然后在 base potential 上叠加 pulse amplitude，再采样得到 I_pulse。

1.4 最终记录 dI = I_pulse - I_base。

1.5 串口输出的数据行格式为：

DATA,t_ms,we_channel,segment,index,e_base_mV,e_pulse_mV,i_base_uA,i_pulse_uA,dI_uA,valid

1.6 上位机后续应该把 DATA 行保存为 CSV，并用 e_base_mV 作为横轴，dI_uA 作为纵轴。

2 默认参数

2.1 we 默认 1。

2.2 start 默认 -200 mV。

2.3 end 默认 600 mV。

2.4 step 默认 5 mV。

2.5 pulse_amp 默认 50 mV。

2.6 scan_rate 默认 50 mV/s。

2.7 pulse_ms 默认 50 ms。

2.8 quiet_ms 默认 2000 ms。

2.9 settle_ms 默认 20 ms。

2.10 vzero 默认 1100 mV。

2.11 rcal 默认 200 ohm。

2.12 rtia 默认 4000 ohm。

2.13 adc_ref 默认 1.8162 V。

2.14 adc_pga 默认 1.5。

2.15 timeout_ms 默认 500 ms。

2.16 max_points 默认 500。

3 参数意义

3.1 we：选择 ADG704 多路复用器连接的工作电极通道。当前固件开放 WE1、WE2、WE3，WE4 作为硬件预留。

3.2 start：DPV 扫描起始电位，单位 mV。

3.3 end：DPV 扫描结束电位，单位 mV。

3.4 step：每个 DPV 点之间的电位步进，单位 mV。步进越小，曲线越细，但扫描时间更长。

3.5 pulse_amp：叠加在 base potential 上的脉冲幅度，单位 mV。常见 DPV 脉冲幅度为 25 mV 到 100 mV，当前默认 50 mV。

3.6 scan_rate：扫描速率，单位 mV/s。它会影响每个电位点之间的最短周期。

3.7 pulse_ms：脉冲保持时间，单位 ms。常见范围约 10 ms 到 100 ms，当前默认 50 ms。

3.8 quiet_ms：开始扫描前的静置时间，单位 ms。换液、刚接上传感器或电极需要稳定时可以加大。

3.9 settle_ms：base potential 后等待电极和模拟前端稳定的时间，单位 ms。

3.10 vzero：AD5941 LPDAC VZERO，用于设置 LPTIA 工作中心点。

3.11 rcal：板上 RCAL 校准电阻值，单位 ohm。当前原理图中 RCAL 是右边 R6，连接在 RCAL0 和 RCAL1 之间，阻值为 200 ohm。左边 R5 10M 和 C17 220pF 是 RC0_0、RC0_1 的高阻电容网络，不是 RCAL。如果实际 BOM 或万用表测得 R6 不是 200 ohm，应在上位机里改成真实值。

3.12 rtia：LPTIA 跨阻电阻，单位 ohm。电流小可以增大，电流大应减小，避免 ADC 饱和。

3.13 adc_ref：ADC reference 电压，单位 V。默认 1.8162 V 来自 ADI 示例和常见校准值，精密标定时可以用实测值。

3.14 adc_pga：ADC PGA 增益。信号小时可以增大，但要避免 ADC 输入饱和。

3.15 timeout_ms：等待一次 FIFO 样本的最长时间。若上位机收到 SAMPLE_TIMEOUT，可以适当增大。

3.16 max_points：本次扫描最多输出多少点，防止参数设置错误导致扫描时间过长。

4 AD5941 采样率说明

4.1 AD5941 的 ADC 原始采样率在低功耗或正常模式下为 800 kSPS。

4.2 高功耗模式下 ADC 可到 1.6 MSPS，但这主要用于较高频率阻抗测量，当前 DPV 固件不使用。

4.3 ADC 后面有 SINC3 和 SINC2 数字滤波器。滤波器会降低输出数据率，同时降低噪声。

4.4 当前 Amperometric 配置使用 SINC3 OSR 4 和 SINC2 OSR 22，主要目的是稳定获得电流数据，而不是追求 ADC 极限速度。

4.5 DPV 的实际点速主要由 pulse_ms、settle_ms、scan_rate 和每点 base/pulse 两次采样决定。

4.6 当前默认参数下，真实 DPV 点速通常远低于 115200 串口能承载的 100 到 150 点/s。

4.7 因此当前固件默认 115200 baud，是为了稳定和兼容上位机。后续确认稳定后，可考虑 230400 或 460800。

5 串口上位机建议

5.1 上位机启动后先打开串口 115200 baud。

5.2 上位机等待 #EVENT,BOOT 和 #IDENTITY。

5.3 如果 #IDENTITY 中 ok 为 1，说明 AD5941 通信正常。

5.4 上位机发送 PARAM? 获取当前默认参数。

5.5 用户在上位机界面修改参数后，上位机逐条发送 SET key value。

5.6 用户点击开始后，上位机发送 START。

5.7 上位机收到 #HEADER 后开始准备接收 DATA。

5.8 每收到一行 DATA，上位机解析并绘图，同时写入 CSV。

5.9 用户点击停止后，上位机发送 STOP。

5.10 上位机收到 #EVENT,BATCH_END 后关闭本次 CSV 文件。

6 无效数据说明

6.1 如果电极未放入溶液，或者输入开路，AD5941 前端可能饱和或漂移。

6.2 这种情况下 i_base_uA、i_pulse_uA 或 dI_uA 可能输出 nan。

6.3 DATA 行最后一列 valid 为 0 时，上位机应显示该点无效，不应参与定量计算。

6.4 入液或接入假负载后，正常情况下 valid 应变为 1，并出现有限电流值。

7 常见错误

7.1 AD5941_NOT_READY：身份寄存器读取失败。应检查 AD5941 供电、CS、SCLK、MOSI、MISO，以及 TF 模块是否还接在共享 MISO 上并上电。

7.2 SAMPLE_TIMEOUT：规定时间内 FIFO 没有数据。可以增大 timeout_ms、settle_ms 或 pulse_ms，也应检查 AD5941 GPIO0 中断和前端状态。

7.3 APP_AMP_INIT：Amperometric 初始化失败。应先看错误码，再检查参数是否越界和 AD5941 是否 ready。

7.4 FIRST_APP_AMP_INIT：开始扫描时第一次 AppAMPInit 失败。可能是 AD5941 状态异常、参数异常或前端未正确初始化。

7.5 CANNOT_SET_WHILE_RUNNING：扫描过程中不允许修改参数。应先 STOP，再 SET，再 START。

8 当前不用 TF 卡的原因

8.1 当前 MicroSD Card Adapter 单独使用可以读写。

8.2 AD5941 单独使用可以正常读取 0x4144 和 0x5502。

8.3 但两者同时连接到同一组 SPI 且 TF 模块上电时，即使 TF_CS 保持高电平，AD5941 仍可能读到 0xFFFFFFFF。

8.4 当拔掉 TF 模块 5V 后，AD5941 立即恢复正常。

8.5 因此当前硬件组合不适合让该 TF 模块和 AD5941 共用 MISO。

8.6 当前寄出版本禁用 TF 卡，避免生化实验室现场因为 TF 模块导致 AD5941 不工作。

9 当前不用 WiFi 的原因

9.1 WiFi 网页方案需要处理热点、局域网 IP、手机浏览器、断线重连和完整数据下载。

9.2 这些功能可以做，但当前阶段会增加现场不确定性。

9.3 上位机串口方案更直接，也更适合标定：参数界面、实时曲线、数据保存、错误提示都可以由电脑软件完成。

9.4 后续如果需要恢复 WiFi，建议先保留串口作为备用数据通道。

10 后续计划

10.1 制作上位机参数界面，包含所有 DPV 参数。

10.2 制作上位机实时曲线，横轴为 e_base_mV，纵轴为 dI_uA。

10.3 制作上位机 CSV 保存，文件头保存参数和设备身份。

10.4 制作上位机错误提示，让实验人员可以按提示检查电极、供电和连接。

10.5 如果未来必须使用 TF 卡，可以更换支持多从机 SPI 共享的 3.3V TF 模块，或把 TF MISO 改到独立 GPIO，并使用软件 SPI。

10.6 如果未来必须使用手机，可以恢复 ESP32 AP 网页模式，并增加网页下载 CSV 功能。
11 本次电流 nan 问题说明

11.1 如果 DATA 里面 e_base_mV、e_pulse_mV、index 都正常增加，但是 i_base_uA、i_pulse_uA、dI_uA 全是 nan，说明串口和 DPV 扫描循环是正常的，问题在 AD5941 电流数据换算阶段。
11.2 本次发现 Amperometric 例程在不重新校准 RTIA 的分支中，会把内部 RTIA 的校准值覆盖为外部 RTIA 数值。当前硬件使用内部 RTIA，外部 RTIA 数值为 0，所以后续电流换算可能出现除以 0，最终输出 nan 或 inf。
11.3 代码已经改为内部 RTIA 时保留校准值；如果校准值异常，则使用参数 rtia 对应的内部 RTIA 标称值兜底，避免整段数据全是 nan。
11.4 valid 仍然保留。valid 为 1 表示 i_base_uA、i_pulse_uA、dI_uA 都是有限数；valid 为 0 表示该点仍有饱和、开路、接线、溶液或电极状态异常等风险。
12 多 WE 分时扫描串口协议

12.1 下位机现在支持上位机一次选择多个 WE，然后自动分时扫描并上传数据。下位机不是同时采 3 个 WE，而是通过 ADG704BRMZ 依次切换 WE。

12.1.1 当前分时方式是按点轮询。选择 WE1、WE2、WE3 时，数据顺序是 WE1 point0、WE2 point0、WE3 point0、WE1 point1、WE2 point1、WE3 point1。这样上位机可以实时显示多个 WE，看起来类似同时采集。

12.1.2 一个 segment 内所有被选择的 WE 按点轮询完成后，才进入下一个 segment。

12.1.3 下位机每完成一个 WE 的一个点就立刻上传一行 DATA。上位机不要等整个 segment 结束再显示，应该边接收边画图。

12.1.4 多 WE 轮询时，单个 WE 的实际点间隔会变长。真实采样时间以上传的 t_ms 为准，scan_rate 是目标值，不保证在多 WE 下完全达到单 WE 速度。

12.2 选择 WE 列表。

12.2.1 只选 WE1。
SET we 1

12.2.2 选择 WE1 和 WE2。
SET wes 1,2

12.2.3 选择 WE1、WE2、WE3。
SET wes 1,2,3

12.2.4 当前固件只开放 WE1、WE2、WE3。WE4 是 ADG704 的硬件预留通道，上位机不要发送 WE4。

12.3 设置所有 WE 共用的 System Setup 参数。

SET adc_ref 1.8162
SET adc_pga 1.5
SET vzero 1100
SET rcal 200
SET rtia 4000
SET timeout_ms 500
SET max_points 500

12.4 设置所有 WE 相同的 DPV Setup 参数。如果上位机的 DPV 参数对所有 WE 一样，可以直接发送下面这些命令。

SET start -200
SET end 600
SET step 5
SET pulse_amp 50
SET scan_rate 50
SET pulse_ms 50
SET quiet_ms 2000
SET settle_ms 20
SET segments 2

12.5 设置某一个 WE 独立的 DPV Setup 参数。如果不同 WE 需要不同方法参数，上位机使用 we1、we2、we3 前缀。

12.5.1 WE1 单独设置。
SET we1.start -200
SET we1.end 600
SET we1.step 5
SET we1.pulse_amp 50
SET we1.scan_rate 50
SET we1.pulse_ms 50
SET we1.quiet_ms 2000
SET we1.settle_ms 20
SET we1.segments 2

12.5.2 WE2 单独设置。
SET we2.start -100
SET we2.end 700
SET we2.step 5
SET we2.pulse_amp 50
SET we2.scan_rate 50
SET we2.pulse_ms 50
SET we2.quiet_ms 2000
SET we2.settle_ms 20
SET we2.segments 2

12.5.3 WE3 单独设置。
SET we3.start -200
SET we3.end 600
SET we3.step 5
SET we3.pulse_amp 50
SET we3.scan_rate 50
SET we3.pulse_ms 50
SET we3.quiet_ms 2000
SET we3.settle_ms 20
SET we3.segments 2

12.6 启动批次扫描。

START

12.7 停止批次扫描。

STOP

12.8 下位机启动后会输出 PARAM 和 WE_PARAM。上位机可以用 PARAM? 主动读取当前配置。

PARAM?

12.9 新 DATA 数据格式如下。

DATA,t_ms,we_channel,segment,index,e_base_mV,e_pulse_mV,i_base_uA,i_pulse_uA,dI_uA,valid

12.10 字段意义如下。

12.10.1 t_ms 是 ESP32-C3 从上电到当前点的毫秒数。
12.10.2 we_channel 是当前数据来自哪个 WE。
12.10.3 segment 是当前 WE 的第几轮重复扫描，从 1 开始。
12.10.4 index 是当前 segment 内的点序号，从 0 开始。
12.10.5 e_base_mV 是基础电位。
12.10.6 e_pulse_mV 是脉冲电位。
12.10.7 i_base_uA 是基础电位采到的电流。
12.10.8 i_pulse_uA 是脉冲电位采到的电流。
12.10.9 dI_uA 等于 i_pulse_uA 减 i_base_uA。
12.10.10 valid 为 1 表示该点三个电流值都是有效数字；valid 为 0 表示该点存在 nan、inf、饱和、开路或采样异常风险。

12.11 上位机推荐流程如下。

12.11.1 打开串口，波特率 115200。
12.11.2 等待 BOOT 和 IDENTITY 信息。
12.11.3 发送 System Setup 参数。
12.11.4 发送 WE 列表，例如 SET wes 1,2,3。
12.11.5 发送每个 WE 的 DPV Setup 参数。如果所有 WE 参数相同，发送不带 we 前缀的 SET start、SET end 等即可。如果每个 WE 参数不同，发送 SET we1.start、SET we2.start、SET we3.start 等。
12.11.6 发送 PARAM? 并记录返回的 PARAM 和 WE_PARAM，写入 CSV 或实验记录。
12.11.7 发送 START。
12.11.8 按 HEADER 解析 DATA，并用 we_channel 和 segment 分组画图、保存。
12.11.9 收到 BATCH_END 后结束本次批次保存。
13 上位机必须处理的错误和警告

13.1 下位机现在会把输入错误、参数越界、启动前配置错误和采样错误上传给上位机。上位机不能只看 DATA，也要监听 #ERROR 和 #WARN。

13.2 错误格式如下。

#ERROR,code,message

13.3 警告格式如下。警告不一定阻止扫描，但上位机应该显示给用户。

#WARN,code,more_fields...

13.4 成功设置参数时，下位机返回如下格式。上位机收到 #OK 后再更新界面里的“已应用参数”状态。

#OK,SET,key,value

13.5 输入格式错误。

13.5.1 SET_FORMAT 表示 SET 命令格式错误。正确格式是 SET key value，例如 SET start -200。
13.5.2 UNKNOWN_COMMAND 表示命令不存在。上位机不应该发送 HELP、STATUS、PARAM?、ID?、START、STOP、SET 之外的命令。
13.5.3 LINE_TOO_LONG 表示单行串口命令超过 120 个字符。上位机应该缩短命令，不要一次发送很长的备注。

13.6 参数输入错误。

13.6.1 PARAM_NOT_NUMBER 表示用户输入的值不是数字。例如 SET start abc 会被拒绝。上位机应该弹窗提示用户重新输入数字。
13.6.2 PARAM_RANGE 表示参数超出允许范围。下位机现在不会偷偷夹到范围内，而是拒绝设置，让用户明确修改。
13.6.3 UNKNOWN_PARAM 表示参数名写错。例如 SET pusle_ms 50 会被拒绝，因为正确写法是 SET pulse_ms 50。
13.6.4 WE_LIST_EMPTY 表示没有选择任何 WE。
13.6.5 WE_LIST_RANGE 表示 WE 列表超范围。当前固件只支持 WE1、WE2、WE3，所以上位机不要发送 WE4。

13.7 START 前配置错误。

13.7.1 ALREADY_RUNNING 表示设备正在扫描，不能再次 START。上位机应该先 STOP 或等待 BATCH_END。
13.7.2 NO_WE_SELECTED 表示没有选择 WE，必须至少选择 WE1、WE2、WE3 中的一个。
13.7.3 CONFIG_START_END_EQUAL 表示某个 WE 的 start 和 end 相同，没有扫描区间。
13.7.4 CONFIG_BAD_STEP 表示 step 不是正数。
13.7.5 CONFIG_MAX_POINTS_TOO_SMALL 表示 max_points 小于该 WE 从 start 到 end 按 step 计算出来的点数。比如 -200 到 600，step 5，需要 161 个点，max_points 必须至少 161。
13.7.6 CONFIG_PULSE_OUT_OF_RANGE 表示 e_base 加 pulse_amp 后超过安全电位范围。上位机应该提示用户减小 pulse_amp 或缩小 start/end。
13.7.7 AD5941_NOT_READY 表示 AD5941 没有准备好，通常是 SPI、供电、TF 模块共用 MISO 干扰或 AD5941 ID 读取失败。

13.8 运行中错误。

13.8.1 AD5941_IDENTITY_FAILED 表示读取不到 AD5941 ID。常见原因是 AD5941 没供电、SPI 接线错误、TF 模块仍然接在共享 MISO 上。
13.8.2 APP_AMP_INIT 或 FIRST_APP_AMP_INIT 表示 AD5941 官方 Amperometric 初始化失败。上位机应该停止本次实验，并显示错误码给用户。
13.8.3 SAMPLE_TIMEOUT 表示规定时间内没有等到 FIFO 数据。可以增大 timeout_ms、settle_ms 或 pulse_ms，也要检查 AD5941 GPIO0 中断和前端状态。

13.9 scan_rate 警告。

13.9.1 SCAN_RATE_LIMITED 表示用户设置的 scan_rate 太快，低于 settle_ms 加 pulse_ms 加必要开销所能达到的最短点周期。下位机仍会扫描，但实际速度会变慢。多 WE 轮询时这个情况更容易出现。
13.9.2 上位机遇到 SCAN_RATE_LIMITED 时，可以提示用户减小 scan_rate，或减小 settle_ms、pulse_ms，或接受实际速度变慢。

13.10 上位机建议的防呆规则。

13.10.1 文本框输入前先在上位机本地验证数字，不要把 abc、空字符串、中文单位直接发给下位机。
13.10.2 WE 通道输入只允许 1、2、3，多个通道用英文逗号，例如 1,2,3。
13.10.3 start、end、step、pulse_amp、scan_rate、pulse_ms、quiet_ms、settle_ms、segments 可以每个 WE 单独设置；adc_ref、adc_pga、vzero、rcal、rtia、timeout_ms、max_points 是 System Setup，所有 WE 共用。
13.10.4 发送每个 SET 后等待 #OK 或 #ERROR。不要连续猛发很多命令后不看返回，否则用户输入错误时很难定位。
13.10.5 发送 START 前建议先发送 PARAM?，把 #PARAM 和 #WE_PARAM 保存到 CSV 文件头或实验记录里。
13.10.6 收到 #ERROR 后，上位机应停止继续发送 START，弹窗显示 code 和 message，让用户修改参数。
13.10.7 收到 DATA 且 valid 为 0 时，不要删除该行。保存原始数据，但画图或峰值计算时可以标记为无效点。

14 100 kΩ 假负载验证和 ADG704 EN 逻辑

14.1 用 100 kΩ 假负载验证时，WE1 接电阻一端，电阻另一端接 RE 和 CE 的短接点。推荐参数是 start 0 mV、end 200 mV、step 20 mV、pulse_amp 0 mV、settle_ms 100 ms、pulse_ms 50 ms、segment 1。

14.2 理论电流由外部 100 kΩ 决定。0 mV 约 0 uA，100 mV 约 1 uA，200 mV 约 2 uA。电流符号可能因为方向定义显示为负，但绝对值应接近线性关系。

14.3 RTIA 参数是 AD5941 内部 TIA 反馈电阻，不是外部 100 kΩ 假负载。外部 100 kΩ 用来产生已知电流，内部 RTIA 用来把这个电流转换成 ADC 能测的电压。

14.4 ADG704BRMZ 的 EN 为 1 时使能当前 A1/A0 选择的通道，EN 为 0 时所有 S 通道断开。如果 EN 逻辑写反，接 100 kΩ 时不会出现理论电流，只会看到接近 0 的噪声漂移。

14.5 如果 100 kΩ 测试仍然只有 nA 级电流，上位机或实验人员应检查 WE1 是否真的接到 S1，ADG704 的 D 是否接 SE0，RE 和 CE 是否真的短接，电阻是否确实为 100 kΩ，固件是否已经烧录最新版本。

14.6 固件会在每个 WE 的每个 segment 初始化后主动打印 RTIA 调试信息，不需要上位机额外发送命令。格式如下。

#INFO,rtia_debug,tag,segment_init,we,1,segment,1,rtia_selected_ohm,10000.000,rtia_calc_ohm,10000.000

14.7 rtia_selected_ohm 是上位机设置的 RTIA 参数，rtia_calc_ohm 是 AD5941 官方 Amperometric 代码实际用于电流换算的 RTIA 校准值。如果 100 kΩ 假负载下电流比例差很多，应优先检查 rtia_calc_ohm 是否明显偏离 rtia_selected_ohm。

14.8 实测发现，如果上位机设置 RTIA 为 10000 ohm，但官方 RTIA 校准返回约 254671 ohm，则 100 kΩ 假负载 200 mV 的理论 2 uA 会被错误换算成约 0.08 uA。固件已经增加保护：内部 RTIA 校准值如果低于标称值的 0.5 倍或高于标称值的 2 倍，就自动改用标称 RTIA，避免电流比例被严重算错。

15 RTIA 来源、正式测量和校准诊断

15.1 正式 DPV 测量界面不要把外部 RTIA 当作普通测量选项开放。正式测量推荐只使用内部 RTIA，因为内部 RTIA 已经通过 100 kΩ 假负载验证，0 到 200 mV 能得到接近欧姆定律的线性电流。

15.2 上位机 System Setup 的正式测量区域应显示内部 RTIA 设置。用户输入 RTIA ohm 后，上位机发送如下命令。

SET rtia_source internal
SET rtia 10000

15.3 上位机可以新增一个校准或硬件诊断页面，用来测试板子上的外部 RTIA 电阻。这个页面必须明确提示：外部 RTIA 仅用于硬件诊断和校准验证，暂不用于正式 DPV。

15.4 校准或硬件诊断页面可以提供 4 个 RTIA 来源选项。

15.4.1 内部 RTIA。

15.4.2 外部 RTIA AIN3 2 kΩ。

15.4.3 外部 RTIA AIN2 25.5 kΩ。

15.4.4 外部 RTIA AIN1 470 kΩ。

15.5 选择外部 RTIA AIN3 2 kΩ 时，上位机隐藏或禁用 RTIA ohm 输入框，只发送来源。

SET rtia_source ext_ain3_2k

15.6 选择外部 RTIA AIN2 25.5 kΩ 时，上位机隐藏或禁用 RTIA ohm 输入框，只发送来源。

SET rtia_source ext_ain2_25k5

15.7 选择外部 RTIA AIN1 470 kΩ 时，上位机隐藏或禁用 RTIA ohm 输入框，只发送来源。

SET rtia_source ext_ain1_470k

15.8 固件接受的 RTIA 来源别名如下，建议上位机只使用标准写法，避免以后维护混乱。

15.8.1 internal。

15.8.2 ext_ain3_2k。

15.8.3 ext_ain2_25k5。

15.8.4 ext_ain1_470k。

15.9 固件输出的参数信息新增如下字段。

#PARAM,rtia_source,internal
#PARAM,rtia_internal_ohm,10000.000
#PARAM,rtia_effective_ohm,10000.000
#PARAM,rtia_usage,dpv_measurement

15.10 rtia_source 表示来源。rtia_internal_ohm 表示内部 RTIA 模式下准备使用的内部 RTIA 设置值。rtia_effective_ohm 表示当前真正用于电流换算的 RTIA。rtia_usage 表示这个来源是否适合正式 DPV。

15.11 当 rtia_usage 为 dpv_measurement 时，表示可以用于正式 DPV。当 rtia_usage 为 diagnostic_only 时，表示只用于校准或硬件诊断，正式测量界面不应允许用户直接启动。

15.12 外部 RTIA 模式下，rtia_internal_ohm 仍然会保留上一次内部 RTIA 输入值，但它不参与当前外部 RTIA 换算。上位机界面应以 rtia_effective_ohm 和 rtia_usage 为准。

15.13 RTIA 调试信息新增 rtia_source 字段。格式如下。

#INFO,rtia_debug,tag,segment_init,we,1,segment,1,rtia_source,ext_ain2_25k5,rtia_selected_ohm,25500.000,rtia_calc_ohm,25500.000

15.14 如果选择外部 RTIA，rtia_selected_ohm 和 rtia_calc_ohm 应该等于对应的固定值。AIN3 为 2000，AIN2 为 25500，AIN1 为 470000。如果这里打印不对，不要继续做诊断实验，先检查固件是否烧录成功、上位机是否发送了正确命令。

15.15 固件在外部 RTIA 模式收到 START 时，会输出如下警告。上位机必须显示这个警告。

#WARN,EXTERNAL_RTIA_DIAGNOSTIC_ONLY,source,ext_ain2_25k5,message,use_internal_rtia_for_formal_dpv

15.16 Arduino 串口助手手动验证外部 RTIA 的推荐流程如下。

15.16.1 先不要接 TF 卡模块，保持串口 115200 baud。

15.16.2 用 100 kΩ 假负载连接 WE1 到 RE 和 CE 的短接点。

15.16.3 选择 WE1。

SET we 1

15.16.4 设置假负载验证用 DPV 参数。

SET start 0
SET end 200
SET step 20
SET pulse_amp 0
SET scan_rate 50
SET pulse_ms 50
SET quiet_ms 1000
SET settle_ms 100
SET segments 1
SET timeout_ms 1000
SET max_points 500

15.16.5 先测内部 RTIA。

SET rtia_source internal
SET rtia 10000
START

15.16.6 再测外部 AIN2 25.5 kΩ，这一档比较适合先验证。

SET rtia_source ext_ain2_25k5
START

15.16.7 如果 AIN2 验证正常，再分别测 AIN3 2 kΩ 和 AIN1 470 kΩ。

SET rtia_source ext_ain3_2k
START

SET rtia_source ext_ain1_470k
START

15.17 判断是否正常。

15.17.1 0 mV 附近电流应接近 0 uA。

15.17.2 100 mV 附近电流绝对值应接近 1 uA。

15.17.3 200 mV 附近电流绝对值应接近 2 uA。

15.17.4 pulse_amp 为 0 时，dI_uA 理论上应接近 0，因为 base 和 pulse 电位相同。真正用于验证欧姆定律的是 i_base_uA 和 i_pulse_uA。

15.18 当前已知实测结果。

15.18.1 内部 RTIA 10000 ohm 配合外部 100 kΩ 假负载，100 mV 附近约 1 uA，200 mV 附近约 2 uA，结果正常。

15.18.2 外部 AIN2 25.5 kΩ 配合外部 100 kΩ 假负载，实测出现约 42 uA 固定偏置，不随 0 到 200 mV 线性变化，暂不用于正式 DPV。

15.18.3 外部 AIN1 470 kΩ 配合外部 100 kΩ 假负载，实测出现约 2.3 uA 固定偏置，不随 0 到 200 mV 线性变化，暂不用于正式 DPV。

15.19 如果内部 RTIA 正常而某个外部 RTIA 不正常，说明问题多半在外部 RTIA 接法、AD5941 外部 RTIA 开关路径、AIN 脚连接或该档电阻焊接上，不要直接认为 DPV 算法错误。

15.20 如果三个外部 RTIA 全部不正常，而内部 RTIA 正常，下一步应重点核对 AD5941 数据手册中 LPTIA 外部 RTIA 的接入脚和当前原理图 AIN1、AIN2、AIN3 到 SE0 的电阻接法是否完全匹配。
