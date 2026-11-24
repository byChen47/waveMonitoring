# waveMonitoring — 独立波浪监测模块

[English](README.md) | **中文**

[![License: GPL v3](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
[![OpenFOAM](https://img.shields.io/badge/OpenFOAM-v2206%2B-2b6cb0.svg)](https://www.openfoam.com)
[![Tested](https://img.shields.io/badge/tested-v2606%20%7C%20gcc%209.5-2f855a.svg)](#9-验证记录本机实测)

从 waves2Foam 里**单独剥离**出来的“测量波浪”部分：**不再需要 waves2Foam 的求解器，也不链接 waves2Foam 的任何库**，
只要把 `libwaveMonitoring.so` 用 `libs` 加载进 `controlDict`，任何带 VOF 场（`alpha.*`）的求解器都能用。

```
libwaveMonitoring.so  ──▶  waveElevation   自由面高程时程（function object）
GaugesNProbes         ──▶  probeDefinitions 批量生成测点 → functions 片段
waveSpectra           ──▶  谱分析 / 统计量（独立后处理程序）
```

### 快速开始

```bash
source ~/OpenFOAM/OpenFOAM-v2606/etc/bashrc        # 任意 OpenFOAM v2206+
git clone https://github.com/byChen47/waveMonitoring.git
cd waveMonitoring && ./Allwmake                    # 库 + waveSpectra + GaugesNProbes
```

算例的 `system/controlDict` 里加：

```cpp
functions
{
    waveElevation1
    {
        type            waveElevation;
        libs            ("libwaveMonitoring.so");
        locations       ((0.10 0.05 0.005) (0.25 0.05 0.005) (0.40 0.05 0.005));
        writeControl    timeStep;
        writeInterval   1;
    }
}
```

跑完做谱分析：

```bash
waveSpectra -file postProcessing/waveElevation/waveElevation1/elevation.dat \
            -out waveSpectra
```

或者用 `probeDefinitions` 批量布点：

```bash
GaugesNProbes -case .      # 然后在 functions 里加 #include "gaugesNProbes.functions"
```

---

## 1. 为什么单独拆出来

waves2Foam 原来的测波链路是：

```
sampledSurfaceElevation (waves2FoamSampling)   ← 需要 waves2Foam 的版本目录
      ↓ 原始 alpha 采样
postProcessWaves2Foam  (waves2FoamProcessing)  ← 链接 libwaves2Foam + GSL
      ↓ eta(t)
powerSpectraLS / reflectionAnalysis            ← 谱分析
```

问题：整条链路跟着 waves2Foam 的构建体系走（`WAVES_*` 环境、`bin/bashrc`、`-lwaves2Foam`），
换个求解器（比如你自己的 `interFoam`、`overInterFoam`、带多孔挡板的晃荡求解器）就不方便用。

本模块把“测波”重写成**只依赖 OpenFOAM 本身**：

| 能力 | waves2Foam 原实现 | 本模块 |
|---|---|---|
| 自由面高程 | `surfaceElevation`（写原始 alpha 采样，需二次插值） | `waveElevation`（**直接**输出 η(t)，一个文件） |
| 谱分析 | `postProcessWaves2Foam`（链接 `-lwaves2Foam -lgsl`） | `waveSpectra`（只链接 `-lOpenFOAM`） |
| 依赖 | `libwaves2Foam.so`、`libwaves2FoamProcessing.so`、GSL | 只要 OpenFOAM |

**可以和 waves2Foam 同时存在**：类型名不同（`waveElevation` vs `surfaceElevation`），不会重复注册。

---

## 2. 依赖

- OpenFOAM v2206+（本模块在 **v2606 / g++ 9.5** 上编译并实测）
- 算例里有 `alpha` 场（VOF，默认 `alpha.water`）和 `g`（`constant/g`）
- 不需要 GSL，不需要 waves2Foam

## 3. 编译

```bash
source ~/OpenFOAM/OpenFOAM-v2606/etc/bashrc
cd waveMonitoring          # 仓库根目录
./Allwmake
```

产物：

```
$FOAM_USER_LIBBIN/libwaveMonitoring.so
$FOAM_USER_APPBIN/waveSpectra
```

清理：`./Allwclean`

---

## 4. 使用：在任意求解器里测波

在 `system/controlDict` 里加（`functions { }` 已存在就加进去）：

```cpp
functions
{
    waveElevation1
    {
        type            waveElevation;
        libs            ("libwaveMonitoring.so");

        // 必填：测点坐标（射线方向上的位置，一般放在你关心的水面附近）
        locations       ((0.10 0.05 0.005) (0.25 0.05 0.005) (0.40 0.05 0.005));

        // 可选（括号内为默认值）
        alpha               alpha.water;   // VOF 场名
        liquid              true;          // alpha=1 是液体
        direction           (0 0 0);       // (0 0 0) → 用 g 的反方向
        interpolationScheme cellPoint;     // alpha 的插值格式
        subtractReference   false;         // true → 输出相对第一次写出的起伏
        log                 true;          // 屏幕打印

        // 采样频率由这里控制（跟求解器写出时刻无关）
        writeControl    timeStep;
        writeInterval   1;
    }
}
```

然后正常运行求解器即可，例如：

```bash
interFoam
```

输出（单文件、逐行累积）：

```
postProcessing/waveElevation/waveElevation1/elevation.dat
```

格式：

```
# waveElevation: free-surface elevation (integrated liquid height)
# functionObject: waveElevation1
# alpha: alpha.water
# liquid: 1
# direction: (0 0 0)
# subtractReference: 0
# nGauges: 3
# location 0: (0.1 0.05 0.005)
# location 1: (0.25 0.05 0.005)
# location 2: (0.4 0.05 0.005)
# time	eta0	eta1	eta2
0.00119904	0.290476	0	0
0.00263651	0.290458	1.28997e-21	1.11626e-66
...
```

- 第 1 列是时间，后面每列对应一个 `locations` 测点；
- 表头带测点坐标，文件自描述，`waveSpectra` 直接能读；
- 并行算例只在 master 写（数值是全处理器归约后的结果）。

### 参数说明

| 参数 | 含义 | 默认 |
|---|---|---|
| `locations` | 测点坐标列表（vectorList），必填 | — |
| `alpha` | VOF 场名 | `alpha.water` |
| `liquid` | `true`：α=1 为液体；`false`：α=1 为气体 | `true` |
| `direction` | 界面法向（射线方向），一般不用给，用 `-g` | `(0 0 0)` |
| `interpolationScheme` | α 在测点上的插值格式 | `cellPoint` |
| `subtractReference` | 是否减去第一次写出的高程 | `false` |
| `fixedLocations` | 测点固定在空间（`true`）还是随网格/物体移动（`false`，相对位置不变） | `true` |
| `log` | 是否在 log 里打印每次采样 | `true` |
| `writeControl`/`writeInterval` | 采样频率（继承 functionObject） | `timeStep`? 见 OF 默认 |

### 固定测点 vs 随体移动测点（`fixedLocations`）

和 OpenFOAM 自带 `probes` 的语义一致：

| `fixedLocations` | 行为 |
|---|---|
| `true`（默认） | 测点**固定在空间**；网格运动时每次重新搜索所在单元，测到的始终是同一空间位置的水位 |
| `false` | 测点在创建时“绑定”到所在单元，之后**跟着网格/物体一起运动**（相对位置不变），适合装在运动物体（晃荡舱、浮体、多孔挡板）上的波高仪 |

实现方式：`false` 时记录测点所在单元的中心 `C0`，每次写出用 `C(t) - C0` 作为位移（并行下按处理器归约，取找到该单元的处理器的位移），
`true` 时不做任何跟踪。拓扑变化（如 AMR、`dynamicRefineFCL`）后会自动按最后位置重新绑定。

```cpp
movingGauge
{
    type            waveElevation;
    libs            ("libwaveMonitoring.so");
    locations       ((0.25 0.05 0.0025));
    fixedLocations  false;      // 随物体运动
    writeControl    timeStep;
    writeInterval   1;
    log             true;       // 打开后 log 里会打印当前测点坐标
}
```

**验证**（静水槽整体做 `x = 0.05 sin(10t)` 平移，同一初始位置放一个固定测点和一个移动测点，0.6 s / 43 个样本）：

```
固定测点 : log 中无 locations，水位随空间位置变化
移动测点 : x - 0.05 sin(10t) = 0.250000 ± 1.2e-6 m
           相对初始位置最大偏差 6.1e-7 m，eta 波动 5e-6 m
```

### 高程是怎么算的

沿 `-direction`（默认竖直向上）穿过测点做一条射线，对液体体积分数做线积分：

```
eta = ∫ alpha dl
```

对平底水槽，这正好是**自由面到槽底的高度**。相比“找 alpha=0.5 等值面交点”，
积分法在掺气、破碎、飞溅时更稳（不会因为找不到单一交点而跳变）。

> 如果用 `direction` 指定了别的方向，等价高度就是沿该方向的水柱长度。

---

## 5. 用 `probeDefinitions` 批量生成波高仪（`GaugesNProbes`）

手写 `locations` 适合少量测点；测点一多（晃荡、多孔挡板那种几十个测点），用
`GaugesNProbes` 从 `constant/probeDefinitions` 批量生成。它是 waves2Foam 的
`waveGaugesNProbes` 的独立替代，**只依赖 OpenFOAM**。

```bash
cd <case>
GaugesNProbes -case .          # 读 constant/probeDefinitions
# → system/gaugesNProbes.functions  +  gaugesNProbes/<name>.vtk
```

然后在 `system/controlDict` 的 `functions {}` 里加一行：

```cpp
functions
{
    #include "gaugesNProbes.functions"
}
```

### `probeDefinitions` 关键字（沿用 waves2Foam，现有文件可直接用）

| 关键字 | 含义 |
|---|---|
| `type` | `waveGauge` → 生成 `waveElevation`；`probeGauge` → 生成 OF `probes` |
| `pointDistribution` | 测点分布类型（见下） |
| `N` | 该分布的测点数 |
| `linestart` / `lineend` / `stretch` | `lineDistribution`：直线布点（`stretch` 可选） |
| `centre` / `radius` / `axis` | `circularDistribution`：圆周布点 |
| `linestart0` / `lineend0` / `lineend1` / `N0` / `N1` / `stretch0` / `stretch1` | `quadrilateralDistribution`：四边形面布点 |
| `xValues` / `yValues` / `zValues` | `userDefinedDistribution`：直接给坐标，Field 语法如 `nonuniform 3(0.1 0.2 0.3)` |
| 子字典 | `concatenatedDistribution`：把多个分布拼起来 |
| `add` | 每个波高仪竖直线的起点→终点偏移（用于 VTK 显示） |
| `fixedLocations` | `false` = 测点随网格/物体移动（默认 `true`，固定），会写进生成的对象 |
| `writeControl` / `writeInterval` | 写进生成的对象（可选，默认 `timeStep` / `1`） |
| `alpha` | `waveGauge` 用的 VOF 场名（可选，默认 `alpha.water`） |
| `fields` | `probeGauge` 采样的场列表（可选，默认 `alpha.water`） |

示例（`examples/probeDefinitions` 可直接拷去改）：

```cpp
gaugeRow
{
    type                waveGauge;
    pointDistribution   lineDistribution;
    N                   3;
    linestart           (0.10 0.05 0.0025);
    lineend             (0.50 0.05 0.0025);
    add                 (0 0.30 0);
    axis                y;
    writeControl        timeStep;
    writeInterval       1;
}

pointProbes
{
    type                probeGauge;
    pointDistribution   userDefinedDistribution;
    N                   2;
    xValues             nonuniform 2(0.2 0.3);
    yValues             nonuniform 2(0.1 0.1);
    zValues             nonuniform 2(0.0025 0.0025);
    fields              (alpha.water U);
}
```

生成的 `system/gaugesNProbes.functions`（可直接改）：

```cpp
    gaugeRow
    {
        type            waveElevation;
        libs            ("libwaveMonitoring.so");
        locations
        (
            (0.1 0.05 0.0025)
            (0.3 0.05 0.0025)
            (0.5 0.05 0.0025)
        );
        alpha           alpha.water;
        writeControl    timeStep;
        writeInterval   1;
        log             false;
    }

    pointProbes
    {
        type            probes;
        libs            (sampling);
        probeLocations
        (
            (0.2 0.1 0.0025)
            (0.3 0.1 0.0025)
        );
        fields          (alpha.water U);
        writeControl    timeStep;
        writeInterval   1;
    }
```

### 命令行选项

| 选项 | 默认 | 说明 |
|---|---|---|
| `-case <dir>` | `.` | 算例目录 |
| `-dict <file>` | `constant/probeDefinitions` | 布局文件 |
| `-out <file>` | `system/gaugesNProbes.functions` | 生成的 functions 片段 |
| `-functionObject <type>` | `waveElevation` | `waveGauge` 用的对象类型 |
| `-libs <lib>` | `libwaveMonitoring.so` | 该对象所在的库 |
| `-noVTK` | 关 | 不写 `gaugesNProbes/<name>.vtk` |

> 生成文件里每个分组名 = function object 名，所以 `probeDefinitions` 里的名字要唯一。
> 跑完求解器：波高仪数据在 `postProcessing/waveElevation/<name>/elevation.dat`，
> 点探针数据在 `postProcessing/<name>/<time>/<field>`（OF `probes` 的格式）。

## 6. 后处理：谱分析与统计量

```bash
waveSpectra [-file <elevation.dat>] [-out <dir>]
            [-tStart <t>] [-tEnd <t>] [-df <df>] [-fMax <f>]
            [-gauge <i>|all]
```

| 参数 | 含义 | 默认 |
|---|---|---|
| `-file` | 输入文件 | `postProcessing/waveElevation/waveElevation1/elevation.dat` |
| `-out` | 输出目录 | `waveSpectra` |
| `-tStart` / `-tEnd` | 分析时间窗（跳过造波/瞬态段） | 全记录 |
| `-df` | 频率分辨率 | `1/T`（T=记录长度） |
| `-fMax` | 最高分析频率 | `1/(2dt)`（Nyquist） |
| `-gauge` | 只分析某个测点（0,1,2…） | `all` |

输出：

```
waveSpectra/
├── waveStats.dat          # 汇总表：Hm0, Tp, Tm02, Tm01, H1/3, T1/3, Hmax, Hmean, mwl, 波数
├── eta0_spectrum.dat      # 每个测点一份：f  S(f)（单边方差密度）
├── eta1_spectrum.dat
└── ...
```

统计量：

| 符号 | 含义 | 算法 |
|---|---|---|
| `Hm0` | 谱有效波高 | `4*sqrt(m0)` |
| `Tp` | 谱峰周期 | `1/f(S_max)` |
| `Tm02` | 谱平均周期 | `sqrt(m0/m2)` |
| `Tm01` | 谱平均周期（一阶矩） | `m0/m1` |
| `H1/3`, `T1/3` | 有效波高/周期（时域） | 上跨零点分波，取最高 1/3 平均 |
| `Hmax`, `Hmean` | 最大/平均波高 | 同上 |
| `mwl` | 平均水位 | 记录均值 |

**数值校验**（合成正弦波 A=0.05 m, T=1.2 s, 60 s 记录, dt=0.01 s）：

```
理论: Hm0 = 2*sqrt(2)*A = 0.14142   H1/3 = 2A = 0.1   Tp = Tm02 = T1/3 = 1.2
实测: Hm0 = 0.141398                H1/3 = 0.1        Tp = Tm02 = T1/3 = 1.200
```


### 参数用法（逐条示例）

所有参数都可省略。下面每条命令都在 `examples/spectra` 的同一条合成记录上实测
（120 s、dt = 0.02 s、Hs = 0.05 m、Tp = 1.2 s）。

| 目的 | 命令 |
|---|---|
| 默认：读默认路径、全记录、自动 `df`/`fMax` | `waveSpectra` |
| 显式给输入和输出目录 | `waveSpectra -file elevation.dat -out out` |
| 只分析 20–80 s（跳过启动瞬态） | `waveSpectra -file elevation.dat -tStart 20 -tEnd 80` |
| 截掉高频，统计量只用到 6 Hz | `waveSpectra -file elevation.dat -fMax 6` |
| 只输出第 1 个测点 | `waveSpectra -file elevation.dat -gauge 0` |
| 完整组合 | `waveSpectra -file <elevation.dat> -out spectra -tStart 5 -fMax 6 -gauge 2` |

**① 什么都不给** —— 在算例目录里直接跑，自动读默认路径：

```bash
cd <case>
waveSpectra
```

```
waveSpectra: read 6001 samples, 1 gauge(s) from postProcessing/waveElevation/waveElevation1/elevation.dat
  window: t = 0 .. 120  (6001 samples, dt = 0.02 s)
  df = 0.00833333 1/s   fMax = 25 1/s
  out: waveSpectra
...
  eta0	0.050124	1.17647	0.966557	...
```

**② 指定文件与输出目录**

```bash
waveSpectra -file elevation.dat -out out_explicit
```

→ 写入 `out_explicit/waveStats.dat` 和 `out_explicit/eta0_spectrum.dat`。

**③ 只分析一段时间窗**

```bash
waveSpectra -file elevation.dat -tStart 20 -tEnd 80
```

```
  window: t = 20 .. 80  (3001 samples, dt = 0.02 s)
  df = 0.0166667 1/s   fMax = 25 1/s
  eta0	0.0515591	1.17647	0.949221	...
```

`df` 会随窗口长度自动变成 `1/(tEnd-tStart)`。

**④ 限制最高分析频率**

```bash
waveSpectra -file elevation.dat -fMax 6
```

给得比 Nyquist 还大时，程序会自动截断并警告：

```
WARNING: -fMax 60 is above the Nyquist frequency 25 1/s, clamped
```

**⑤ 只算某一个测点**

```bash
waveSpectra -file elevation.dat -gauge 0
```

下标越界会明确报错（不会悄悄写一个空表）：

```
No gauge selected: -gauge 5 but the file has 1 gauge(s) (indices 0..0)
```

**⑥ 组合使用**

```bash
waveSpectra -file postProcessing/waveElevation/gauges/elevation.dat \
            -out spectra -tStart 5 -fMax 6 -gauge 2
```

#### 参数之间的关系（容易踩的坑）

| 现象 | 原因 | 建议 |
|---|---|---|
| 设了更细的 `-df` 后 `Hm0` 偏大（本例 0.072 vs 期望 0.05） | `df < 1/T` 时相邻频率不正交，方差被重复累加 | **一般不要设 `-df`**：默认的 `1/T` 才是正交分解 |
| 设了更粗的 `-df` 后 `Hm0` 偏小（0.0467） | 频率太粗，谱尾部的能量被丢掉 | 同上，用默认值最稳 |
| 改了 `-tStart/-tEnd` 后 `df` 跟着变 | `df` 默认 = `1/(记录长度)` | 知道即可；`Tp` 的偏差量级就是 `df` |
| `-fMax` 超过 `1/(2dt)` | 会混叠 | 程序自动截断到 Nyquist 并警告 |
| `-gauge` 用了不存在的下标 | 文件里只有 N 个测点 | 下标范围 `0..N-1`，与 `eta0/eta1/...` 列一一对应 |

#### 现成配方

```bash
# 造波水槽：跳过前 5 s 的造波过渡
waveSpectra -file postProcessing/waveElevation/gauges/elevation.dat -tStart 5

# 晃荡：记录很长、只关心低频，截到 5 Hz
waveSpectra -file postProcessing/waveElevation/tank/elevation.dat -fMax 5

# 多点阵列：先全部算，再对第 3 个测点单独出谱复核
waveSpectra -file .../elevation.dat -out all
waveSpectra -file .../elevation.dat -gauge 2 -out gauge2
```

### 算例：已知谱的合成波（`examples/spectra`，几秒钟跑完）

`examples/spectra` 自带一个“已知答案”的完整算例：生成一条 JONSWAP 谱的合成自由面时程
（Hs = 0.05 m，Tp = 1.2 s），交给 `waveSpectra` 反算，再与理论值对比。
**不需要求解器、不需要网格**，用来验证谱分析本身。

```bash
cd waveMonitoring/examples/spectra
./Allrun
```

`Allrun` 三步：

1. `makeSignal.py`：用 400 个频率分量合成 `elevation.dat`（格式与 `waveElevation` 的输出一致，
   120 s、dt = 0.02 s），并把离散谱的理论值写进 `targets.txt`；
2. `waveSpectra -file elevation.dat -out waveSpectra`；
3. `checkResults.py`：读 `waveSpectra/waveStats.dat` 与 `targets.txt` 对比，超差返回非 0。

实测输出：

```
  quantity        target       recovered     rel.error
  --------------- ------------ ------------- ----------
  Hm0             0.050000     0.050124      2.48e-03
  Tp              1.194401     1.176470      1.50e-02
  Tm02            0.961586     0.966557      5.17e-03

  tolerance: 3.0%
  VERDICT  : PASS
```

怎么读这个结果：

| 量 | 说明 |
|---|---|
| `Hm0` | 与输入的 `Hs` 差 0.25%，剩下的是频率离散化误差 |
| `Tp` | 1.176 s vs 1.194 s，差 1.5%，量级正好是频率分辨率 `df = 1/120 ≈ 0.0083 1/s` |
| `Tm02` | 0.967 s vs 0.962 s，0.5% |

改参数直接加命令行，例如：

```bash
python3 makeSignal.py --hs 0.1 --tp 1.6 --duration 180
waveSpectra -file elevation.dat -out waveSpectra && python3 checkResults.py
```

对真实 CFD 数据，把 `-file` 指向你的 `postProcessing/waveElevation/<name>/elevation.dat`，
一般再用 `-tStart` 跳过造波/启动瞬态段：

```bash
waveSpectra -file postProcessing/waveElevation/gauges/elevation.dat \
            -out spectra -tStart 5
```

---

## 7. 在任何求解器上使用

只要有 `alpha` 场就能用，与求解器类型无关：

| 求解器 | 说明 |
|---|---|
| `interFoam` / `overInterFoam` | 标准 VOF，直接可用（已用 `interFoam` + damBreak 实测） |
| `porousWaveFoam` / `waveFoam` | 也可用（此时与 waves2Foam 的 `surfaceElevation` 并存不冲突） |
| 自研求解器 | 只要 `alpha.water` 在 `objectRegistry` 里即可 |

**注意**：`libwaveMonitoring.so` 只提供 function object，不做造波/消波；造波仍由你算例的边界条件决定。

### 关于 `locations` 怎么取

- 平底水槽：`(x, z_still_water, 0)` 即可，y 坐标只决定射线穿过哪里（射线会贯穿整个 bbox）；
- 需要多点阵列：直接列出来，或用 OpenFOAM 的 `#calc` / `#eval` 生成；
- 测点必须在网格内。

---

## 8. 已知限制

- 谱分析假设**均匀时间步**（`dt` 取平均）；非均匀输出请先插值到等间隔。
- `userDefinedDistribution` 的坐标必须是 Field 语法（`nonuniform 3(0.1 0.2 0.3)`），
  写 `(0.1 0.2 0.3)` 会报 `Expected keyword 'uniform' or 'nonuniform'`。
- 重启算例会覆盖 `elevation.dat`（同一 function object 名字），要保留请改 `-out`/备份。
- 并行只在 master 写文件，`elevation.dat` 行数与串行一致。
- `waveElevation` 每次写出都重建射线（`meshSearch`），网格很大、测点很多时会有额外开销；
  需要极高频采样时建议 `writeControl timeStep; writeInterval N;` 降低频率。

---

## 9. 验证记录（本机实测）

| 测试 | 结果 |
|---|---|
| 库编译（**纯 OpenFOAM 环境**，不 source waves2Foam） | 通过，生成 `libwaveMonitoring.so` |
| `interFoam` + damBreak + 3 个测点 | 1 s 物理时间，488 行数据；x=0.10 处初始 η=0.2905 m（damBreak 初始水深 0.292 m，✔）；t=1 s 时三点 η≈0.0869/0.0867/0.0500 |
| `waveSpectra` 合成正弦波 | Hm0 误差 0.02%，Tp/Tm02/T1/3 精确 |
| `waveSpectra` 读 damBreak 数据 | 正常输出 `waveStats.dat` + 3 个谱文件 |
| `GaugesNProbes` + `probeDefinitions`（1 组 lineDistribution 3 点 + 1 组 userDefinedDistribution 2 点） | 生成 `system/gaugesNProbes.functions` 与 2 个 `.vtk`；`#include` 后 `interFoam` 输出 `waveElevation/<name>/elevation.dat`（3 列）与 `probes` 的 `alpha.water`/`U`（2 点） |

---

## 10. 目录结构

```
waveMonitoring/
├── Allwmake / Allwclean
├── Make/{files,options}              # libwaveMonitoring.so
├── waveElevation/
│   ├── waveElevation.H
│   └── waveElevation.C               # function object
├── waveSpectra/
│   ├── Make/{files,options}
│   └── waveSpectra.C                 # 后处理程序
├── gaugesNProbes/
│   ├── Make/{files,options}
│   ├── GaugesNProbes.C               # probeDefinitions -> functions 片段
│   └── pointDistributions/           # line/circular/quadrilateral/concatenated/userDefined
└── examples/
    ├── waveElevation.controlDict     # 可直接抄的 functions 片段
    ├── probeDefinitions              # GaugesNProbes 的布局示例
    └── README.md                     # 用 OpenFOAM 自带 damBreak 做 5 分钟试跑
```

---

## 11. 许可与引用

- **许可**：GNU General Public License v3.0，见 [LICENSE](LICENSE)。本模块派生自
  [waves2Foam](https://github.com/ogoe/waves2Foam)（GPL）并构建在 OpenFOAM 之上，沿用同一许可。
- **引用**：用于发表时请引用 waves2Foam 的原始文献：

  > Jacobsen, N.G., Fuhrman, D.R. and Fredsøe, J. (2012). *A Wave Generation Toolbox for the
  > Open-Source CFD Library: OpenFOAM®*. Int. J. Numer. Meth. Fluids, 70(9), 1073–1088.
  > DOI: [10.1002/fld.2726](https://doi.org/10.1002/fld.2726)

---

## 12. 使用须知

- 本模块只负责**测量**：不做造波/消波，波场仍由你算例的边界条件决定。
- 输出的 η(t)、谱文件与统计量适合直接用于论文制图；`waveSpectra` 的数值精度见第 9 节验证。
- 有问题或改进建议，欢迎开 issue / PR。
