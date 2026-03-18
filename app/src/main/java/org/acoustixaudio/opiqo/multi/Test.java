package org.acoustixaudio.opiqo.multi;

import static androidx.core.util.SparseIntArrayKt.contains;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

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

    void printPreset () {
        Log.d(TAG, "printPreset: checking whether printPreset works");
        AudioEngine.printPreset(1);
    }

    void stressTestPlugins () throws JSONException {
        Log.d(TAG, "stressTestPlugins: checking whether plugins crash when controls are changed");
        for (int i = 0; i < mainActivity.pluginUris.size(); i++) {
            AudioEngine.addPlugin(1, mainActivity.pluginUris.get(i));
            Log.d(TAG, "stressTestPlugins: testing plugin " + mainActivity.pluginUris.get(i));
            JSONObject ports = mainActivity.pluginInfo.getJSONObject(mainActivity.pluginUris.get(i)).optJSONObject("ports");
            String key = ports.keys().next();
            while (key != null) {
                JSONObject port = ports.optJSONObject(key);
                if (! port.optString("type").equals("audio") && ! port.optString("type").equals("atom")) {
                    Log.d(TAG, "stressTestPlugins: changing control " + key);
                    int index = port.optInt("index");
                    int min = port.optInt("min");
                    int max = port.optInt("max");
                    for (int value = min; value <= max; value += (max - min) / 10) {
                        AudioEngine.setValue(1, index, value);
                        Log.d(TAG, "stressTestPlugins: set control " + key + " to " + value);
                    }
                }
                
                key = ports.keys().hasNext() ? ports.keys().next() : null;
            }
        }
    } 
}
