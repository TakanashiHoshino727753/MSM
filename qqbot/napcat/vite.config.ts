import { defineConfig } from "vite";

// 库模式打包：把 src/index.ts 打包成单个 index.mjs（NapCat 插件入口）。
// 不引入额外运行时依赖：发送用全局 fetch，长连接用全局 WebSocket（Node ≥ 21 自带）。
export default defineConfig({
  build: {
    target: "node18",
    outDir: "dist",
    emptyOutDir: true,
    lib: {
      entry: "src/index.ts",
      formats: ["es"],
      fileName: () => "index.mjs",
    },
    rollupOptions: {
      // napcat-types 仅用于类型，构建时被擦除；其余全部内联打包
      external: [],
    },
  },
});
