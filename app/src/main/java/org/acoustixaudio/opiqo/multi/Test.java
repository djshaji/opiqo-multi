package org.acoustixaudio.opiqo.multi;

import static androidx.core.util.SparseIntArrayKt.contains;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.util.ArrayList;
import java.util.stream.IntStream;

public class Test {
    MainActivity mainActivity;
    String TAG = "Test";

    Test (MainActivity _mainActivity) {
        mainActivity = _mainActivity;
    }

    static int pluginCounter = 0;
    void pluginLoader () {
        Log.d(TAG, "pluginLoader: checking whether plugins load");
        pluginCounter = 0;
        int [] skip = {};
        ArrayList <String> failedPlugins = new ArrayList<>();
        final Handler handler = new Handler(Looper.getMainLooper());
        Runnable runner = new Runnable() {
            @Override
            public void run() {
                // Code to run after the delay, e.g., start a new activity
                // Intent intent = new Intent(SecondActivity.this, MainActivity.class);
                // startActivity(intent);

                Log.i(TAG, "pluginLoader test [" + pluginCounter + "]: loading " + mainActivity.pluginUris.get(pluginCounter));
                if (IntStream.of (skip).anyMatch(x -> x == pluginCounter)) {
                    Log.w(TAG, "pluginLoader: skipping plugin " + mainActivity.pluginUris.get(pluginCounter));
                    pluginCounter++;
                    handler.postDelayed(this, 100); // Delay in milliseconds
                    return;
                }

                String uri = mainActivity.pluginUris.get(pluginCounter++);

                if (AudioEngine.addPlugin(1, uri) != 0) {
                    Log.e(TAG, "pluginLoader: load [failed]");
                    failedPlugins.add(uri);
                }

                if (mainActivity.pluginUris.size() <= pluginCounter) {
                    handler.removeCallbacks(this);
                    pluginCounter = 0;
                    Log.d(TAG, "run: test ended");
                    Log.d(TAG, "pluginLoader: load [ok]");
                    if (! failedPlugins.isEmpty()) {
                        Log.d(TAG, "pluginLoader: failed plugins: ");
                        for (String failedPlugin : failedPlugins) {
                            Log.d(TAG, "pluginLoader: " + failedPlugin);
                        }
                    } else {
                        Log.i(TAG, "pluginLoader: all plugins loaded successfully");
                    }
                } else {
                    handler.postDelayed(this, 30); // Delay in milliseconds
                }

            }
        };

         handler.postDelayed(runner, 300); // Delay in milliseconds

    }
}
