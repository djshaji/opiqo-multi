package org.acoustixaudio.opiqo.multi;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.util.ArrayList;

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
        ArrayList <String> failedPlugins = new ArrayList<>();
        final Handler handler = new Handler(Looper.getMainLooper());
        Runnable runner = new Runnable() {
            @Override
            public void run() {
                // Code to run after the delay, e.g., start a new activity
                // Intent intent = new Intent(SecondActivity.this, MainActivity.class);
                // startActivity(intent);

                Log.i(TAG, "pluginLoader: loading " + mainActivity.pluginUris.get(pluginCounter));
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
