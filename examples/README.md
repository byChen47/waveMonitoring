# Examples

Two self-contained checks for the module.

## 1. `spectra/` — spectral analysis with a known answer (no solver needed, ~2 s)

```bash
cd examples/spectra
./Allrun
```

A JONSWAP record is synthesised from 400 components, analysed with `waveSpectra` and the
result is compared with the exact values of the discretised spectrum
(Hm0 0.0501 vs 0.05, Tp 1.176 vs 1.194, Tm02 0.967 vs 0.962 — PASS).
Details: [`spectra/README.md`](spectra/README.md).

## 2. `damBreak` — full chain: library + gauges + spectra

用 OpenFOAM 自带的 `damBreak` 走一遍“加载 lib → 测波 → 谱分析”，确认在你机器上是通的。
教程不入库（避免重复拷贝 OpenFOAM 的算例），直接复制过来即可。

```bash
# 0) 环境（只需要 OpenFOAM，不需要 waves2Foam）
source ~/OpenFOAM/OpenFOAM-v2606/etc/bashrc

# 1) 编译模块（只需一次）
cd <path to>/waveMonitoring && ./Allwmake

# 2) 准备算例
cp -r $FOAM_TUTORIALS/multiphase/interFoam/laminar/damBreak/damBreak /tmp/damBreakMon
cd /tmp/damBreakMon
rm -rf [1-9]* 0.[0-9]* postProcessing
cp -r 0.orig 0
```

把 `examples/waveElevation.controlDict` 里的 `waveElevation1` 块加进 `system/controlDict` 的
`functions { ... }` 里（**注意**：是加在已有 `functions` 花括号内部，不要新开一个 `functions`），
再把 `endTime` 改大一点，比如 1.0：

```bash
sed -i 's/endTime .*/endTime         1.0;/' system/controlDict
```

然后：

```bash
blockMesh
setFields        # damBreak 必须！否则全域 alpha=0，测到的高程全是 0
interFoam
```

结果：

```bash
head -12 postProcessing/waveElevation/waveElevation1/elevation.dat
```

应该看到 x=0.10 m 的测点初始 η ≈ 0.2905 m（damBreak 初始水深 0.292 m）。

再做谱分析：

```bash
waveSpectra -file postProcessing/waveElevation/waveElevation1/elevation.dat \
            -out waveSpectra -tStart 0.6
cat waveSpectra/waveStats.dat
```

> damBreak 是衰减的非周期信号，所以 `Tp` 会等于记录长度、`H1/3` 可能只有一个波；
> 这是信号本身的特点，不是程序问题。想验证谱算法，请用 `spectra/`（已知 JONSWAP 谱）
> 或 `README.md` 第 6 节的合成正弦波校验。
