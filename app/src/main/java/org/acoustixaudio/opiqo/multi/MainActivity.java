package org.acoustixaudio.opiqo.multi;

import static android.view.View.GONE;

import android.Manifest;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.CompoundButton;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ToggleButton;

import androidx.activity.EdgeToEdge;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import com.google.android.material.slider.Slider;
import com.google.oboe.samples.audio_device.AudioDeviceListEntry;
import com.google.oboe.samples.audio_device.AudioDeviceSpinner;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = "MainActivity";
    SharedPreferences sharedPreferences;

    JSONObject UIs = new JSONObject();

    static {
        System.loadLibrary("multi");
    }

    private static final int PERMISSION_REQUEST_CODE = 100;
    private ToggleButton onOff;
    private Context context;

    public JSONObject pluginInfo;
    ArrayList <String> pluginNames;
    ArrayList <String> pluginUris;
    HashMap<Integer, View> pluginUIContainers = new HashMap<>();
    private CollectionFragment collectionFragment;
    private AudioDeviceSpinner playbackDeviceSpinner, recordingDeviceSpinner;

    String [] tests = {
            "Plugin Loader Test",
            "Preset Save Test",
            "Preset Load Test"
    };
    private Slider gainSlider;

    void runTest(int index) {
        switch (index) {
            case 0:
                new Test(this).pluginLoader();
                break;
            case 1:
//                new Test(this).printPreset();
                savePreset();
                break;
            case 2:
                loadPreset();
                break;
            default:
                Log.w(TAG, "runTest: no such test: " + index);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        context = this;
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_main);
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });

        sharedPreferences = getSharedPreferences("core", MODE_PRIVATE);

        TextView testButton = findViewById(R.id.power);
        testButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                AlertDialog.Builder builder = new AlertDialog.Builder(context);
                builder.setTitle("Select Test")
                        .setItems(tests, new DialogInterface.OnClickListener() {
                            public void onClick(DialogInterface dialog, int which) {
                                runTest(which);
                            }
                        });
                builder.show();
            }
        });

        gainSlider = findViewById(R.id.volume_slider);
        gainSlider.addOnChangeListener(new Slider.OnChangeListener() {
            @Override
            public void onValueChange(@NonNull Slider slider, float value, boolean fromUser) {
                AudioEngine.setGain(value);
            }
        });

        recordingDeviceSpinner = findViewById(R.id.recording_devices_spinner);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            recordingDeviceSpinner.setDirectionType(AudioManager.GET_DEVICES_INPUTS);
            recordingDeviceSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                @Override
                public void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
                    AudioEngine.setRecordingDeviceId(getRecordingDeviceId());
                }

                @Override
                public void onNothingSelected(AdapterView<?> adapterView) {
                    // Do nothing
                }
            });
        }

        playbackDeviceSpinner = findViewById(R.id.playback_devices_spinner);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            playbackDeviceSpinner.setDirectionType(AudioManager.GET_DEVICES_OUTPUTS);
            playbackDeviceSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                @Override
                public void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
                    AudioEngine.setPlaybackDeviceId(getPlaybackDeviceId());
                }

                @Override
                public void onNothingSelected(AdapterView<?> adapterView) {
                    // Do nothing
                }
            });
        }

        pluginNames = new ArrayList<>();
        pluginUris = new ArrayList<>();

        // Request record audio permission if not already granted
        requestRecordAudioPermission();
        FrameLayout pager_layout = findViewById(R.id.pager_container);

        collectionFragment = new CollectionFragment(this);
        getSupportFragmentManager().beginTransaction()
                .replace(R.id.pager_container, collectionFragment)
                .commit();

        String path = getFilesDir() + "/lv2";
        Log.d(TAG, "onCreate: [lv2 path] " + path);
        copyAssetsToFiles("lv2");

        AudioEngine.create();
        AudioEngine.initPlugins(path);

        try {
            pluginInfo = new JSONObject(AudioEngine.getPluginInfo());
//            Log.d(TAG, "onCreate: [plugin info] " + pluginInfo.toString(2));

            Iterator<String> keys = pluginInfo.keys();
            String plugin = null;
            while(keys.hasNext()) {
                String key = keys.next();
                plugin = key;
                pluginUris.add(pluginInfo.getJSONObject(key).getString("uri"));
                pluginNames.add(pluginInfo.getJSONObject(key).getString("name"));
                if (pluginInfo.get(key) instanceof JSONObject) {
//                    Log.d(TAG, "onCreate: [plugin] + " + key + " : " + pluginInfo.getJSONObject(key).toString(2));
                }
            }


        } catch (JSONException e) {
            throw new RuntimeException(e);
        }


        onOff = findViewById(R.id.onoff);
        onOff.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(@NonNull CompoundButton compoundButton, boolean b) {
                if (ContextCompat.checkSelfPermission(context, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED)
                    requestRecordAudioPermission();
                else
                    AudioEngine.setEffectOn(b);
            }
        });

    }

    /**
     * Request RECORD_AUDIO permission from the user
     */
    private void requestRecordAudioPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
                    != PackageManager.PERMISSION_GRANTED) {
                // Permission is not granted, request it
                ActivityCompat.requestPermissions(this,
                        new String[]{Manifest.permission.RECORD_AUDIO},
                        PERMISSION_REQUEST_CODE);
            }
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
                                          @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);

        if (requestCode == PERMISSION_REQUEST_CODE) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                // Permission granted
                Toast.makeText(this, "Audio permission granted", Toast.LENGTH_SHORT).show();
                if (onOff.isChecked())
                    AudioEngine.setEffectOn(true);
            } else {
                // Permission denied
                Toast.makeText(this, "Audio permission denied", Toast.LENGTH_SHORT).show();
            }
        }
    }

    private String copyAssetsToFiles(String assetDir) {
        File baseDir = getFilesDir();
        try {
            copyAssetDir(getAssets(), assetDir, baseDir);
        } catch (java.io.IOException e) {
            Log.e(TAG, "copyAssetsToFiles failed", e);
        }

        return baseDir.getAbsolutePath();
    }

    private void copyAssetDir(android.content.res.AssetManager am, String assetPath, File outDir) throws java.io.IOException {
        String[] list = am.list(assetPath);
        if (list == null || list.length == 0) {
            // It's a file
            String name = assetPath.contains("/") ? assetPath.substring(assetPath.lastIndexOf('/') + 1) : assetPath;
            File outFile = new File(outDir, name);
            try (java.io.InputStream in = am.open(assetPath);
                 java.io.OutputStream out = new java.io.FileOutputStream(outFile)) {
                byte[] buf = new byte[8192];
                int r;
                while ((r = in.read(buf)) != -1) {
                    out.write(buf, 0, r);
                }
            }
        } else {
            // It's a directory
            File targetDir = outDir;
            if (!assetPath.isEmpty()) {
                String name = assetPath.contains("/") ? assetPath.substring(assetPath.lastIndexOf('/') + 1) : assetPath;
                targetDir = new File(outDir, name);
                if (!targetDir.exists()) targetDir.mkdirs();
            }
            for (String name : list) {
                String child = assetPath.isEmpty() ? name : assetPath + "/" + name;
                copyAssetDir(am, child, targetDir);
            }
        }
    }

    public void showAddPluginDialog(View root, View add, int position) {
        AlertDialog.Builder builder;

        builder = new AlertDialog.Builder(this);
        CharSequence[] pluginNamesArray = pluginNames.toArray(new CharSequence[0]);
        builder.setTitle("Add Plugin")
                .setItems(pluginNamesArray, new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int which) {
                        // The 'which' argument contains the index position of the selected item.
                        String pluginUri = pluginUris.get(which);
                        AudioEngine.addPlugin(position, pluginUri);
                        Log.d(TAG, "[add plugin]: " + position + ":" + pluginUri);
                        UI pluginUI = new UI(context, pluginInfo.optJSONObject(pluginUri).toString(), position);
                        pluginUI.add = add;

                        try {
                            UIs.put(String.valueOf(position), pluginUI);
                        } catch (JSONException e) {
                            Toast.makeText(context, e.getMessage(), Toast.LENGTH_SHORT).show();
                            throw new RuntimeException(e);
                        }

                        LinearLayout layout = (LinearLayout) root;
                        layout.removeAllViews();

                        layout.addView(pluginUI);
                        add.setVisibility(GONE);
                    }
                });

        builder.show();
    }


    private int getRecordingDeviceId(){
        return ((AudioDeviceListEntry)recordingDeviceSpinner.getSelectedItem()).getId();
    }

    private int getPlaybackDeviceId(){
        return ((AudioDeviceListEntry)playbackDeviceSpinner.getSelectedItem()).getId();
    }

    @Override
    protected void onStop() {
        onOff.setChecked(false);
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        onOff.setChecked(false);
        super.onDestroy();
    }

    void savePreset () {
        String preset = AudioEngine.getPresetList();
        SharedPreferences.Editor editor = sharedPreferences.edit();
        editor.putString("default_preset", preset);
        editor.apply();

        Log.d(TAG, "savePreset: " + preset);
    }

    void loadPreset () {
        String preset = sharedPreferences.getString("default_preset", null);
        if (preset == null) {
            Log.w(TAG, "loadPreset: no preset found");
            return;
        }

        JSONObject presetJson, pluginInfoCopy;
        try {
            pluginInfoCopy = new JSONObject(pluginInfo.toString());
            presetJson = new JSONObject(preset);
        } catch (JSONException e) {
            Log.e(TAG, "loadPreset: failed to parse preset", e);
            return;
        }

        Log.d(TAG, "loadPreset: loading " + presetJson);

        gainSlider.setValue((float) presetJson.optDouble("gain", 0f));
        for (int j = 1 ; j < 5 ; j ++) {
//            collectionFragment.tabLayout.getTabAt(j-1).select();
//            collectionFragment.viewPager.setCurrentItem(j, false);
            JSONObject plugin1 = presetJson.optJSONObject("plugin" + j);
            if (plugin1 != null) {
                String uri = plugin1.optString("uri", null);
                JSONObject info = pluginInfoCopy.optJSONObject(uri);
                Log.d(TAG, "loadPreset: " + info);
                JSONArray ports = info.optJSONArray("port");
                for (int i = 0; i < ports.length(); i++) {
                    JSONObject port = ports.optJSONObject(i);
                    if (!port.optString("type").equals("audio")) {
                        try {
                            port.put("default", plugin1.getJSONObject("controls").get(port.optString("name")));
                        } catch (JSONException e) {
                            Toast.makeText(context, e.getMessage(), Toast.LENGTH_SHORT).show();
                            throw new RuntimeException(e);
                        }
                        Log.d(TAG, "loadPreset: [port] " + port);
                    }
                }

                try {
                    info.put("port", ports);
                } catch (JSONException e) {
                    Toast.makeText(context, e.getMessage(), Toast.LENGTH_SHORT).show();
                    throw new RuntimeException(e);
                }

                UI pluginUI = new UI(context, info.toString(), j);
                /*
                if (!pluginUIContainers.containsKey(j)) {
                    collectionFragment.viewPager.setCurrentItem(j);
                    Log.d(TAG, "loadPreset: " + "waiting for plugin container " + j);
                    while (!pluginUIContainers.containsKey(j)) {
                        try {
                            Thread.sleep(100);
                        } catch (InterruptedException e) {
                            Toast.makeText(context, e.getMessage(), Toast.LENGTH_SHORT).show();
                            throw new RuntimeException(e);
                        }
                    }
                }

                 */

                LinearLayout container = (LinearLayout) pluginUIContainers.get(j);
                assert container != null;
                container.removeAllViews();
                container.addView(pluginUI);
                pluginUI.add = ((ConstraintLayout) container.getParent()).findViewById(R.id.add);
                pluginUI.add.setVisibility(GONE);
            }
        }
    }
}