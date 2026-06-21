package cc.opendw.dragonwars;

import android.os.Bundle;
import android.content.res.AssetManager;
import java.io.*;
import org.libsdl.app.SDLActivity;

// 火龍之戰 remake — Android 進入點(scaffold;見 android/README.md)。
//   引擎以相對 cwd 載 assets/(bundle/fonts/i18n);Android assets 封在 APK 不能 fopen,
//   故首次啟動把 APK assets/ 解壓到 getFilesDir(),引擎用該路徑(由 native 端 chdir,
//   或經 DWR_ASSET_DIR 環境變數;見移植計畫 §2)。
public class MainActivity extends SDLActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        try { extractAssets("", new File(getFilesDir(), "assets")); } catch (IOException e) { e.printStackTrace(); }
        // DWR_ASSET_DIR 要指向「**含 assets/ 的目錄**」(main.cpp chdir 後找相對路徑 assets/bundle)。
        //   extractAssets 把 APK assets/ 解到 getFilesDir()/assets/,故這裡傳 getFilesDir()(parent),
        //   chdir 後 "assets/bundle" → getFilesDir()/assets/bundle(正確)。傳 .../assets 會變雙重 assets。
        try { android.system.Os.setenv("DWR_ASSET_DIR", getFilesDir().getAbsolutePath(), true); } catch (Throwable t) {}
        // 存檔目錄(feasibility §4.2):internal storage 下獨立 save/(可寫,不混進資產目錄)。
        //   不設的話引擎在 Android 的 fallback 可能寫不到(無 APPDATA/XDG/可寫 HOME)。
        try {
            File saveDir = new File(getFilesDir(), "save");
            saveDir.mkdirs();
            android.system.Os.setenv("DWR_SAVE_DIR", saveDir.getAbsolutePath(), true);
        } catch (Throwable t) {}
        super.onCreate(savedInstanceState);
    }
    // 遞迴把 APK assets/<path> 解壓到 dst（已存在則略過,加快二次啟動）。
    private void extractAssets(String path, File dst) throws IOException {
        AssetManager am = getAssets();
        String[] list = am.list(path);
        if (list == null || list.length == 0) {   // 檔案
            dst.getParentFile().mkdirs();
            if (dst.exists()) return;
            try (InputStream in = am.open(path); OutputStream out = new FileOutputStream(dst)) {
                byte[] buf = new byte[8192]; int n; while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            }
            return;
        }
        for (String c : list) {                    // 目錄
            String cp = path.isEmpty() ? c : path + "/" + c;
            extractAssets(cp, new File(dst, c));
        }
    }
}
