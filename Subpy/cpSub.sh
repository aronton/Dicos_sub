#!/bin/bash
#SBATCH --partition=edr1-al9_long_serial
#SBATCH --job-name=combinePBC
#SBATCH --cpus-per-task=10
#SBATCH --output=outputPBC/combinePBC_%j.out

date

source /dicos_ui_home/aronton/.bashrc

# 確保 output 資料夾存在
mkdir -p outputPBC

# 總任務數
TOTAL=300
# 每輪同時跑幾個
BATCH=10

for ((i=1; i<=TOTAL; i+=BATCH))
do
    echo "🔄 Starting round from $i to $((i+BATCH-1 > TOTAL ? TOTAL : i+BATCH-1)) ..."
    
    start_time=$(date +%s)  # 記錄開始時間

    for ((j=0; j<BATCH && i+j<=TOTAL; j++))
    do
        idx=$((i + j))
        srun --exclusive --nodes=1 --ntasks=1 --cpus-per-task=10 \
             --output=outputPBC/combinePBC_${idx}.out \
             python /dicos_ui_home/aronton/tSDRG_random/Subpy/combine.py $idx &
    done

    wait  # 等待這一輪所有 srun 結束

    end_time=$(date +%s)  # 記錄結束時間
    duration=$((end_time - start_time))
    
    echo "✅ Round starting at $i done. Time spent: ${duration} seconds."
    echo
done

# 總結
echo "🎉 All jobs completed at:"
date