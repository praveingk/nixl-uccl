SOURCE_DIR="/home/yanggepkuer15/nixl"

TARGET_MACHINES=("10.202.15.196")

TARGET_DIR="${SOURCE_DIR}"

if [ ! -d "$SOURCE_DIR" ]; then
  exit 1
fi

for MACHINE in "${TARGET_MACHINES[@]}"; do
  (
    rsync -avz -e 'ssh -o StrictHostKeyChecking=no' --delete "$SOURCE_DIR/" "${USER}@$MACHINE:$TARGET_DIR/"
  ) &
done

wait

echo "Done."