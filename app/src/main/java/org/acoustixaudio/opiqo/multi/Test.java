package org.acoustixaudio.opiqo.multi;

import static androidx.core.util.SparseIntArrayKt.contains;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.widget.LinearLayout;
import android.widget.Spinner;

import com.google.android.material.slider.Slider;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.stream.IntStream;

public class Test {
    MainActivity mainActivity;
    String TAG = "Test";
    int TEST_CASES = 50;

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
        Iterator<String> keys = mainActivity.pluginInfo.keys();
        String plugin = null;
        while(keys.hasNext()) {
            String key = keys.next();
            plugin = key;
            AudioEngine.addPlugin(1, key);
            Log.d(TAG, "--------------------------------");
            Log.d(TAG, "stressTestPlugins: testing plugin " + key);
            Log.d(TAG, mainActivity.pluginInfo.getJSONObject(key).toString(2));
            JSONArray ports = mainActivity.pluginInfo.getJSONObject(key).getJSONArray("port");
            for (int i = 0; i < ports.length(); i++) {
                JSONObject port = ports.getJSONObject(i);
                Log.d(TAG, "stressTestPlugins: " + port);
                if (! port.has("type") || port.getString("type").equals("audio") || port.getString("type").equals("atom")) {
                    continue;
                }

                Log.d(TAG, "stressTestPlugins: testing control " + port.getString("name"));
                int min = port.getInt("min");
                int max = port.getInt("max");
                int range = max - min;
                for (int j = 0; j < 50; j++) {
                    int value = min + (int)(Math.random() * range);
//                    Log.d(TAG, "stressTestPlugins: setting control " + port.getString("name") + " to " + value);
                    AudioEngine.setValue(1, port.getInt("index"),  value);
                }
            }

             AudioEngine.deletePlugin(1);
        }
    }

    void stressTestPluginUI () throws JSONException {
        Log.d(TAG, "stressTestPluginUI: checking whether plugins crash when UI is opened and closed");
        Iterator<String> keys = mainActivity.pluginInfo.keys();
        LinearLayout container = (LinearLayout) mainActivity.pluginUIContainers.get(1);

        while (keys.hasNext()) {
            String key = keys.next();
            JSONObject info = mainActivity.pluginInfo.getJSONObject(key);

            mainActivity.addPlugin(info, 1);
            try {
                Thread.sleep(1000);
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }

            for (int j = 0; j < 5; j++) {
                Log.d(TAG, "--------------------------------");
                Log.d(TAG, String.format("[test %d/5] %s", j, key));
                mainActivity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        assert container != null;
                        UI pluginUI = container.getChildAt(0) instanceof UI ? (UI) container.getChildAt(0) : null;
                        testSliders(pluginUI != null ? pluginUI.sliders : new HashMap<>());
                        testSpinners(pluginUI != null ? pluginUI.spinners : new HashMap<>());
                        try {
                            Thread.sleep(10);
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                        }
                    }
                });
            }

            Log.d(TAG, "test [ok]: finished testing plugin " + key);
        }
    }

    void testSliders (HashMap<String, Slider> sliders) {
        String [] keys = sliders.keySet().toArray(new String[0]);
        for (String key : keys) {
            Slider slider = sliders.get(key);
            Log.d(TAG, "testSliders: testing slider " + key);
            for (int i = 0; i < TEST_CASES; i++) {
                assert slider != null;
                float value = slider.getValue();
                float min = slider.getValueFrom();
                float max = slider.getValueTo();
                float range = max - min;
                float newValue = min + (float) (Math.random() * range);
                mainActivity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Log.d(TAG, "run: setting slider " + key + " to " + newValue);
                        slider.setValue(newValue);
                        try {
                            Thread.sleep(10);
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                        }
                    }
                });
            }
        }
    }

    void testSpinners (HashMap <String, Spinner> spinners) {
        String [] keys = spinners.keySet().toArray(new String[0]);
        for (String key : keys) {
            Spinner spinner = spinners.get(key);
            Log.d(TAG, "testSpinners: testing spinner " + key);
            if (spinner.getAdapter() == null) {
                Log.w(TAG, "testSpinners: spinner " + key + " has no adapter, skipping");
                continue;
            }

            int count = spinner.getAdapter().getCount();
            for (int i = 0; i < TEST_CASES; i++) {
                int newValue = (int) (Math.random() * count);
                int finalI = i;
                mainActivity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Log.d(TAG, "[spinner " + finalI + "/50] " + key + " to " + newValue);
                        spinner.setSelection(newValue);
                        try {
                            Thread.sleep(10);
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                        }
                    }
                });
            }
        }
    }

    void gxAmpTest () {
        Log.d(TAG, "gxAmpTest: checking whether GxAmp plugin works");
        AudioEngine.stressTest();
    }
}
