package org.acoustixaudio.opiqo.multi;

import android.util.Log;

import java.util.ArrayList;

public class Test {
    MainActivity mainActivity;
    String TAG = "Test";

    Test (MainActivity _mainActivity) {
        mainActivity = _mainActivity;
    }

    void pluginLoader () {
        Log.d(TAG, "pluginLoader: checking whether plugins load");
        ArrayList <String> failedPlugins = new ArrayList<>();
         for (int i = 0 ; i < mainActivity.pluginUris.size(); i ++) {
             Log.i(TAG, "pluginLoader: loading " + mainActivity.pluginUris.get(i));
             String uri = mainActivity.pluginUris.get(i);
             if (AudioEngine.addPlugin(1, uri) != 0) {
                Log.e(TAG, "pluginLoader: load [failed]");
                failedPlugins.add(uri);
             }

             Log.d(TAG, "pluginLoader: load [ok]");
             Log.d(TAG, "pluginLoader: failed plugins: ");
                for (String failedPlugin : failedPlugins) {
                    Log.d(TAG, "pluginLoader: " + failedPlugin);
                }
         }
    }
}
