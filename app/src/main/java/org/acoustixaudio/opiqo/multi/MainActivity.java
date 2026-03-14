package org.acoustixaudio.opiqo.multi;

import static android.view.View.GONE;

import android.Manifest;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ToggleButton;

import androidx.activity.EdgeToEdge;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.slider.Slider;
import com.google.oboe.samples.audio_device.AudioDeviceListEntry;
import com.google.oboe.samples.audio_device.AudioDeviceSpinner;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = "MainActivity";
    SharedPreferences sharedPreferences;
    String presetsDir;

    ArrayList <JSONObject> presets = new ArrayList<>();

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
    private File presetsDirectory;
    private TextView patchLabel;
    private ActivityResultLauncher<String[]> persistentPicker;
    private static class PendingFileRequest {
        final int position;
        final String controlUri;
        final String range;
        final String type;
        final String pluginName;
        final String controlName;

        PendingFileRequest(int position, String pluginName, String controlName, String controlUri, String range, String type) {
            this.position = position;
            this.controlUri = controlUri;
            this.range = range;
            this.type = type;
            this.pluginName = pluginName;
            this.controlName = controlName;
        }
    }

    private PendingFileRequest pendingFileRequest;

    void runTest(int index) {
        switch (index) {
            case 0:
                new Test(this).pluginLoader();
                break;
            case 1:
                savePresetToFileWithFilePicker();
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

        // Must be registered before STARTED; chooseFile() only launches this instance.
        persistentPicker = registerForActivityResult(
                new ActivityResultContracts.OpenDocument(),
                uri -> {
                    if (uri == null || pendingFileRequest == null) return;
                    PendingFileRequest req = pendingFileRequest;
                    pendingFileRequest = null; // consume once

                    final int takeFlags = Intent.FLAG_GRANT_READ_URI_PERMISSION
                            | Intent.FLAG_GRANT_WRITE_URI_PERMISSION;
                    try {
                        getContentResolver().takePersistableUriPermission(uri, takeFlags);
                    } catch (SecurityException e) {
                        Log.w(TAG, "chooseFile: persist permission not granted for " + uri, e);
                    }

                    Log.d(TAG, "chooseFile: selected file: " + uri);
                    String path = copyFileToFilesDir(uri, req.pluginName, req.controlName);
                    AudioEngine.setFilePath(req.position, req.controlUri, path);
                }
        );

        sharedPreferences = getSharedPreferences("core", MODE_PRIVATE);
        presetsDir = getFilesDir() + "/presets";
        presetsDirectory = new File(presetsDir);
        if (!presetsDirectory.exists() && !presetsDirectory.mkdirs()) {
            Log.e(TAG, "Failed to create presets directory: " + presetsDir);
        }

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

        ImageButton nextPresetButton = findViewById(R.id.patch_up);
        nextPresetButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                nextPreset();
            }
        });
        ImageButton previousPresetButton = findViewById(R.id.patch_down);
        previousPresetButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                previousPreset();
            }
        });

        ImageButton savePresetButton = findViewById(R.id.save_patch);
        savePresetButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                savePresetToFileWithFilePicker();
            }
        });

        patchLabel = findViewById(R.id.patch_label);
        patchLabel.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                selectPresetFromLoadedPresetsWithDialog();
            }
        });

        patchLabel.setLongClickable(true);
        patchLabel.setOnLongClickListener(new View.OnLongClickListener() {
            @Override
            public boolean onLongClick(View view) {
                if (patchLabel.getText().toString().isEmpty()) {
                    Toast.makeText(context, "No preset to delete", Toast.LENGTH_SHORT).show();
                    return true;
                }
                deletePreset(patchLabel.getText().toString());
                return true;
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

        loadAllPresetsFromPresetsDir();
//        setFirstPreset();
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
        super.onStop();
//        onOff.setChecked(false);
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

    void savePresetFromString (String preset) {
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

        AudioEngine.deletePlugin(1);
        AudioEngine.deletePlugin(2);
        AudioEngine.deletePlugin(3);
        AudioEngine.deletePlugin(4);

        deleteUIs();

        JSONObject presetJson, pluginInfoCopy;
        try {
            String name = new JSONObject(preset).optString("name", "Unnamed Preset");
            patchLabel.setText(name);
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

    void savePresetToFile (String filename) {
        String preset = AudioEngine.getPresetList();
        JSONObject presetJson ;
        try {
            presetJson = new JSONObject(preset);
            presetJson.put("name", filename);
            preset = presetJson.toString();
        } catch (JSONException e) {
            Log.e(TAG, "savePresetToFile: failed to parse preset", e);
            Toast.makeText(this, "Failed to save preset: " + e.getMessage(), Toast.LENGTH_SHORT).show();
            return;
        }

        File outFile = new File(presetsDirectory, filename);
        try (java.io.OutputStream out = new java.io.FileOutputStream(outFile)) {
            out.write(preset.getBytes());
            Toast.makeText(this, "Preset saved to " + outFile.getAbsolutePath(), Toast.LENGTH_SHORT).show();
        } catch (java.io.IOException e) {
            Log.e(TAG, "savePresetToFile failed", e);
            Toast.makeText(this, "Failed to save preset: " + e.getMessage(), Toast.LENGTH_SHORT).show();
        }

        loadAllPresetsFromPresetsDir();
    }

    void savePresetToFileWithFilePicker () {
        View dialogView = LayoutInflater.from(this).inflate(R.layout.file_chooser, null);
        EditText presetName = dialogView.findViewById(R.id.preset_name);

        ImageButton clear = dialogView.findViewById(R.id.clear_name);
        clear.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                presetName.setText("");
            }
        });

        presetName.setText(((TextView) findViewById(R.id.patch_label)).getText());

        new AlertDialog.Builder(this)
                .setTitle("Save preset")
                .setView(dialogView)
                .setPositiveButton("Save", (dialog, which) -> {
                    String filename = presetName.getText().toString().trim();
                    if (filename.isEmpty()) {
                        Toast.makeText(this, "Please enter a preset name", Toast.LENGTH_SHORT).show();
                        return;
                    }

//                    if (!filename.endsWith(".json")) {
//                        filename += ".json";
//                    }

                    savePresetToFile(filename);
                    patchLabel.setText(filename);
                })
                .setNegativeButton("Cancel", null)
                .show();
    }

    void loadAllPresetsFromPresetsDir () {
        File[] files = presetsDirectory.listFiles();
        if (files == null) return;

        presets = new ArrayList<>();

        for (File file : files) {
            try (java.io.InputStream in = new java.io.FileInputStream(file)) {
                byte[] bytes = new byte[(int) file.length()];
                in.read(bytes);
                String preset = new String(bytes);
                presets.add(new JSONObject(preset));
                Log.d(TAG, "loadAllPresetsFromPresetsDir: read preset " + preset);
            } catch (java.io.IOException | JSONException e) {
                Log.e(TAG, "loadAllPresetsFromPresetsDir failed for " + file.getName(), e);
            }
        }
    }

    void selectPresetFromLoadedPresetsWithDialog () {
        CharSequence[] presetNames = new CharSequence[presets.size()];
        for (int i = 0; i < presets.size(); i++) {
            presetNames[i] = presets.get(i).optString("name", "Preset " + (i + 1));
        }

        new AlertDialog.Builder(this)
                .setTitle("Select Preset")
                .setItems(presetNames, (dialog, which) -> {
                    JSONObject selectedPreset = presets.get(which);
                    savePresetFromString(selectedPreset.toString());
                    loadPreset();
                    try {
                        patchLabel.setText(selectedPreset.getString("name"));
                    } catch (JSONException e) {
                        Toast.makeText(context, e.getMessage(), Toast.LENGTH_SHORT).show();
                        throw new RuntimeException(e);
                    }
                })
                .show();
    }

    void deleteUIs () {
        for (int i = 1 ; i < 5 ; i ++) {
            Log.d(TAG, "deleteUIs: " + i);
            LinearLayout layout = (LinearLayout) pluginUIContainers.get(i);
            if (layout != null) {
                layout.removeAllViews();
                View add = ((ConstraintLayout) layout.getParent()).findViewById(R.id.add);
                if (add != null) add.setVisibility(View.VISIBLE);
            }
        }
    }

    void nextPreset () {
        int which = -1;
        String currentPresetName = patchLabel.getText().toString();
        for (int i = 0; i < presets.size(); i++) {
            if (presets.get(i).optString("name", "Preset " + (i + 1)).equals(currentPresetName)) {
                which = i;
                Log.d(TAG, "nextPreset: previous was " + which);
                break;
            }

        }

        if (which == -1) {
            Toast.makeText(this, "Current preset not found in loaded presets", Toast.LENGTH_SHORT).show();
            return;
        }

        if (which == presets.size() - 1) {
            which = 0;
        } else {
            which++;
        }

        JSONObject selectedPreset = presets.get(which);
        savePresetFromString(selectedPreset.toString());
        loadPreset();

        Log.d(TAG, "nextPreset: total presets: " + presets.size());
        Log.d(TAG, "nextPreset: selected: " + which);
    }

    void previousPreset () {
        int which = -1;
        String currentPresetName = patchLabel.getText().toString();
        for (int i = 0; i < presets.size(); i++) {
            if (presets.get(i).optString("name", "Preset " + (i + 1)).equals(currentPresetName)) {
                which = i;
                Log.d(TAG, "previousPreset: previous was " + which);
                break;
            }

        }

        if (which == -1) {
            Toast.makeText(this, "Current preset not found in loaded presets", Toast.LENGTH_SHORT).show();
            return;
        }

        if (which == 0) {
            which = presets.size() - 1;
        } else {
            which--;
        }

        JSONObject selectedPreset = presets.get(which);
        savePresetFromString(selectedPreset.toString());
        loadPreset();

        Log.d(TAG, "previousPreset: total presets: " + presets.size());
        Log.d(TAG, "previousPreset: selected: " + which + " " + selectedPreset);
    }

    void deletePreset (String filename) {
        AlertDialog .Builder builder = new AlertDialog.Builder(this);
        builder.setTitle("Delete Preset")
                .setMessage("Are you sure you want to delete preset " + filename + "?")
                .setPositiveButton("Delete", (dialog, which) -> {
                    File presetFile = new File(presetsDirectory, filename);
                    if (presetFile.delete()) {
                        Toast.makeText(this, "Preset deleted", Toast.LENGTH_SHORT).show();
                        nextPreset();
                        loadAllPresetsFromPresetsDir();
                    } else {
                        Toast.makeText(this, "Failed to delete preset", Toast.LENGTH_SHORT).show();
                    }
                });
        builder.setNegativeButton("Cancel", null);
        builder.show();
    }

    void setFirstPreset () {
        if (presets.size() > 0) {
            JSONObject firstPreset = presets.get(0);
            savePresetFromString(firstPreset.toString());
            loadPreset();
            try {
                patchLabel.setText(firstPreset.getString("name"));
            } catch (JSONException e) {
//                Toast.makeText(context, e.getMessage(), Toast.LENGTH_SHORT).show();
                throw new RuntimeException(e);
            }
        }
    }

    void chooseFileUsingIntent (int position, String controlUri, String range, String type) {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.putExtra("range", range);
        intent.putExtra("controlUri", controlUri);
        intent.setType("*/*");
        startActivityForResult(intent, position);
    }

    /*
    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (data == null || data.getData() == null) return;
        Uri uri = data.getData();
        Log.d(TAG, "onActivityResult: selected file: " + uri);
        String path = copyFileToFilesDir(uri, "plugin" + requestCode, "control" + requestCode);
        String controlUri = data.getStringExtra("controlUri");
        String range = data.getStringExtra("range");
        AudioEngine.setFilePath(requestCode, controlUri, path);
    }

     */

    public void chooseFile (int position, String plugin, String controlName, String controlUri, String range, String type) {
        if (persistentPicker == null) {
            Log.e(TAG, "chooseFile: picker launcher is not initialized");
            return;
        }

        pendingFileRequest = new PendingFileRequest(position, plugin, controlName, controlUri, range, type);

        type = type + "," + type.toUpperCase() + ",application/octet-stream,*/*";
        String[] mimeTypes = (type == null || type.trim().isEmpty())
                ? new String[]{"*/*"}
                : type.split(",");

        Log.d(TAG, "chooseFile: launching file picker for control " + controlUri + " with types " + String.join(", ", mimeTypes));
        persistentPicker.launch(mimeTypes);

    }

    void copyFile (Uri uri, String dstPath) throws java.io.IOException {
        try (java.io.InputStream in = getContentResolver().openInputStream(uri);
             java.io.OutputStream out = new java.io.FileOutputStream(dstPath)) {
            byte[] buf = new byte[8192];
            int r;
            while ((r = in.read(buf)) != -1) {
                out.write(buf, 0, r);
            }
        }
    }

    void copyFile (String srcPath, String dstPath) throws java.io.IOException {
        Log.d(TAG, "copyFile: " + srcPath + " -> " + dstPath);
        try (java.io.InputStream in = new java.io.FileInputStream(srcPath);
             java.io.OutputStream out = new java.io.FileOutputStream(dstPath)) {
            byte[] buf = new byte[8192];
            int r;
            while ((r = in.read(buf)) != -1) {
                out.write(buf, 0, r);
            }
        }
    }

    String copyFileToFilesDir(Uri uri, String plugin, String control) {
        Log.d(TAG, "copyFileToFilesDir: copying file from " + uri + " for plugin " + plugin + " control " + control);
        String filename = uri.getLastPathSegment();
        if (filename == null) {
            Log.e(TAG, "copyFileToFilesDir: failed to get filename from uri " + uri);
            Toast.makeText(context, "failed to get filename", Toast.LENGTH_SHORT).show();
            return null;
        }

        String basename = new File (filename).getName();
        String target = String.format ("%s/user/%s/%s/%s", getFilesDir(), plugin, control, basename);
        File outFile = new File(target);
        outFile.getParentFile().mkdirs();
        Log.d(TAG, "copyFileToFilesDir: [destination] " + target);

        try {
            copyFile(uri, outFile.getAbsolutePath());
            Log.d(TAG, "copyFileToFilesDir: file copied to " + outFile.getAbsolutePath());
        } catch (IOException e) {
            Toast.makeText(context, e.getMessage(), Toast.LENGTH_SHORT).show();
            throw new RuntimeException(e);
        }

        return outFile.getAbsolutePath();
    }

    @Override
    protected void onResume() {
        super.onResume();
//        onOff.setChecked(true);
    }
}