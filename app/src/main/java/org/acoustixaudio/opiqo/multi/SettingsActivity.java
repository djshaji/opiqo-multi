package org.acoustixaudio.opiqo.multi;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.Iterator;

public class SettingsActivity extends AppCompatActivity {
    private static final String TAG = "SettingsActivity";
    private static final int REQUEST_EXPORT_PRESETS = 1;
    private static final int REQUEST_IMPORT_PRESETS = 2;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.settings_activity);
        if (savedInstanceState == null) {
            getSupportFragmentManager()
                    .beginTransaction()
                    .replace(R.id.settings, new SettingsFragment())
                    .commit();
        }
        ActionBar actionBar = getSupportActionBar();
        if (actionBar != null) {
            actionBar.setDisplayHomeAsUpEnabled(true);
        }
    }

    public static class SettingsFragment extends PreferenceFragmentCompat {
        @Override
        public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
            setPreferencesFromResource(R.xml.root_preferences, rootKey);

            Preference export = findPreference("export");
            if (export != null) {
                export.setOnPreferenceClickListener(preference -> {
                    // Let user choose where to save presets JSON.
                    Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    intent.setType("application/json");
                    intent.putExtra(Intent.EXTRA_TITLE, "presets.json");
                    startActivityForResult(intent, REQUEST_EXPORT_PRESETS);

                    return true;
                });
            }

            Preference importPref = findPreference("import");
            if (importPref != null) {
                importPref.setOnPreferenceClickListener(preference -> {
                    // Let user choose presets JSON to import.
                    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    intent.setType("application/json");
                    startActivityForResult(intent, REQUEST_IMPORT_PRESETS);
                    return true;
                });
            }
        }

        @Override
        @SuppressWarnings("deprecation")
        public void onActivityResult(int requestCode, int resultCode, Intent data) {
            super.onActivityResult(requestCode, resultCode, data);
            if (resultCode != RESULT_OK || data == null || data.getData() == null) {
                return;
            }

            SettingsActivity activity = (SettingsActivity) requireActivity();
            Uri uri = data.getData();
            if (requestCode == REQUEST_EXPORT_PRESETS) {
                activity.exportPresetsToUri(uri);
            } else if (requestCode == REQUEST_IMPORT_PRESETS) {
                activity.importPresetsFromUri(uri);
            }
        }
    }

    private void exportPresetsToUri(Uri targetUri) {
        try {
            File presetsDir = new File(getFilesDir(), "presets");
            File[] files = presetsDir.listFiles();
            if (files == null || files.length == 0) {
                Toast.makeText(this, "No presets found to export", Toast.LENGTH_SHORT).show();
                return;
            }

            JSONObject json = new JSONObject();
            for (File file : files) {
                if (!file.isFile()) {
                    continue;
                }
                String content = new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8);
                json.put(file.getName(), content);
            }

            try (OutputStream outputStream = getContentResolver().openOutputStream(targetUri)) {
                if (outputStream == null) {
                    throw new IOException("Unable to open destination file");
                }
                outputStream.write(json.toString(4).getBytes(StandardCharsets.UTF_8));
            }

            Toast.makeText(this, "Presets exported successfully", Toast.LENGTH_SHORT).show();
            Log.d(TAG, "Exported presets count: " + json.length());
        } catch (Exception e) {
            Toast.makeText(this, "Failed to export presets: " + e.getMessage(), Toast.LENGTH_SHORT).show();
            Log.e(TAG, "Failed to export presets", e);
        }
    }

    private void importPresetsFromUri(Uri sourceUri) {
        try (InputStream inputStream = getContentResolver().openInputStream(sourceUri)) {
            if (inputStream == null) {
                throw new IOException("Unable to open selected file");
            }

            String jsonContent = new String(readAllBytes(inputStream), StandardCharsets.UTF_8);
            JSONObject json = new JSONObject(jsonContent);

            File presetsDir = new File(getFilesDir(), "presets");
            if (!presetsDir.exists() && !presetsDir.mkdirs()) {
                throw new IOException("Failed to create presets directory");
            }

            int importedCount = 0;
            Iterator<String> keys = json.keys();
            while (keys.hasNext()) {
                String fileName = keys.next();
                String fileContent = json.optString(fileName, null);
                if (fileContent == null) {
                    continue;
                }

                File targetFile = new File(presetsDir, fileName);
                try (OutputStream outputStream = new FileOutputStream(targetFile)) {
                    outputStream.write(fileContent.getBytes(StandardCharsets.UTF_8));
                    importedCount++;
                }
            }

            Toast.makeText(this, "Imported " + importedCount + " preset(s)", Toast.LENGTH_SHORT).show();
            Log.d(TAG, "Imported presets count: " + importedCount);
        } catch (Exception e) {
            Toast.makeText(this, "Failed to import presets: " + e.getMessage(), Toast.LENGTH_SHORT).show();
            Log.e(TAG, "Failed to import presets", e);
        }

        (MainActivity.getInstance()).loadAllPresetsFromPresetsDir();
    }

    private byte[] readAllBytes(InputStream inputStream) throws IOException {
        byte[] buffer = new byte[4096];
        int read;
        java.io.ByteArrayOutputStream result = new java.io.ByteArrayOutputStream();
        while ((read = inputStream.read(buffer)) != -1) {
            result.write(buffer, 0, read);
        }
        return result.toByteArray();
    }
}