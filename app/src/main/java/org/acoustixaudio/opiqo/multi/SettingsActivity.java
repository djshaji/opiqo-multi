package org.acoustixaudio.opiqo.multi;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;

import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;

import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.SwitchPreference;
import androidx.preference.SwitchPreferenceCompat;
import androidx.preference.DropDownPreference;

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
        } else {
            Log.e(TAG, "onCreate: actionbar is null" );
        }

    }

    public static class SettingsFragment extends PreferenceFragmentCompat {
        @Override
        public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
            setPreferencesFromResource(R.xml.root_preferences, rootKey);

            SwitchPreferenceCompat tips = findPreference("tips");
            if (tips != null) {
                tips.setOnPreferenceChangeListener((preference, newValue) -> {
                    boolean enabled = (boolean) newValue;
                    ((MainActivity) MainActivity.getInstance()).tips = (boolean) newValue;
                    return true;
                });
            }

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

            Preference author = findPreference("contact");
            if (author != null) {
                author.setOnPreferenceClickListener(preference -> {
                    Uri uri = Uri.parse("https://acoustixaudio.org");
                    Intent intent = new Intent(Intent.ACTION_VIEW, uri);
                    startActivity(intent);
                    return false;
                });
            }

            Preference github = findPreference("github");
            if (github != null) {
                github.setOnPreferenceClickListener(preference -> {
                    Uri uri = Uri.parse("https://github.com/djshaji");
                    Intent intent = new Intent(Intent.ACTION_VIEW, uri);
                    startActivity(intent);
                    return false;
                });
            }

            Preference pro = findPreference("pro");
            if (pro != null) {
                pro.setOnPreferenceClickListener(preference -> {
                    Intent intent = new Intent(getActivity(), Purchase.class);
                    startActivity(intent);
                    return true;
                });
            }

            if (MainActivity.proVersion) {
                if (pro != null) {
                    if (MainActivity.proIsBundle)
                        pro.setEnabled(false);
                    pro.setSummary("Pro version activated");
                    pro.setTitle("Premium");
                }
            }

            Preference downloadPc = findPreference("download_pc");
            if (downloadPc != null) {
                downloadPc.setOnPreferenceClickListener(preference -> {
                    Uri uri = Uri.parse("https://opiqo.acoustixaudio.org");
                    startActivity(new Intent(Intent.ACTION_VIEW, uri));
                    return true;
                });
            }

            Preference serialKey = findPreference("bundle_key");
            if (serialKey != null) {
                serialKey.setOnPreferenceClickListener(preference -> {
                    SharedPreferences prefs = requireActivity()
                            .getSharedPreferences("core", Context.MODE_PRIVATE);
                    if (!prefs.getBoolean("is_bundle", false)) {
                        Toast.makeText(getActivity(),
                                "Purchase the Bundle to get your PC version key",
                                Toast.LENGTH_LONG).show();
                        return true;
                    }

                    String key = ((SettingsActivity) requireActivity()).getSerialKey();
                    if (key == null) {
                        Toast.makeText(getActivity(), "Failed to generate key",
                                Toast.LENGTH_SHORT).show();
                        return true;
                    }
                    new AlertDialog.Builder(requireActivity())
                            .setTitle("PC Version Serial Key")
                            .setMessage("Your key (valid for 10 minutes):\n\n" + key)
                            .setPositiveButton("Copy", (d, w) -> {
                                ClipboardManager cm = (ClipboardManager) requireActivity()
                                        .getSystemService(Context.CLIPBOARD_SERVICE);
                                cm.setPrimaryClip(ClipData.newPlainText("serial_key", key));
                                Toast.makeText(getActivity(), "Key copied!",
                                        Toast.LENGTH_SHORT).show();
                            })
                            .setNegativeButton("Close", null)
                            .show();
                    return true;
                });
            }

                            // Processed queue depth preference (controls number of processed blocks kept in the ring buffer)
                            DropDownPreference processed = findPreference("processed_queue_blocks");
                            if (processed != null) {
                                String cur = processed.getValue();
                                if (cur == null || cur.isEmpty()) cur = "2";
                                processed.setSummary("Current: " + cur + " blocks — Lower = lower latency, higher = fewer dropped blocks");
                                processed.setOnPreferenceChangeListener((preference, newValue) -> {
                                    String val = (String) newValue;
                                    int blocks = 2;
                                    try {
                                        blocks = Integer.parseInt(val);
                                    } catch (NumberFormatException e) {
                                        // use default
                                    }
                                    // Apply the new requested depth via native API. If the engine is not created yet
                                    // the native layer will save the requested value and apply it when streams open.
                                    AudioEngine.setProcessedQueueBlocks(blocks);
                                    preference.setSummary("Current: " + val + " blocks — Lower = lower latency, higher = fewer dropped blocks");
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
                String fileContent = json.optString(fileName, "");
                if (fileContent.isEmpty()) {
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

    String getSerialKey() {
        final String base32_key = "JBSWY3DPEHPK3PXP";
        // step    — time step in seconds (default 30)
        // digits  — code length (default 6)
        // skew    — counter offset from current step (0 = now, -1 = previous, +1 = next)
        final int step = 30;    // standard TOTP step — matches Microsoft/Google Authenticator
        final int digits = 6;
        final int skew = 0;

        try {
            byte[] key = base32Decode(base32_key);

            // TOTP counter: floor(epoch_seconds / step) + skew
            long counter = (System.currentTimeMillis() / 1000L) / step + skew;

            // Encode counter as 8-byte big-endian
            byte[] counterBytes = new byte[8];
            long tmp = counter;
            for (int i = 7; i >= 0; i--) {
                counterBytes[i] = (byte) (tmp & 0xff);
                tmp >>= 8;
            }

            // HMAC-SHA1
            Mac mac = Mac.getInstance("HmacSHA1");
            mac.init(new SecretKeySpec(key, "HmacSHA1"));
            byte[] hash = mac.doFinal(counterBytes);

            // Dynamic truncation (RFC 4226)
            int offset = hash[hash.length - 1] & 0x0f;
            int binary = ((hash[offset]     & 0x7f) << 24)
                       | ((hash[offset + 1] & 0xff) << 16)
                       | ((hash[offset + 2] & 0xff) << 8)
                       |  (hash[offset + 3] & 0xff);

            int otp = binary % (int) Math.pow(10, digits);
            return String.format(java.util.Locale.US, "%0" + digits + "d", otp);

        } catch (NoSuchAlgorithmException | InvalidKeyException e) {
            Log.e(TAG, "getSerialKey error", e);
            return null;
        }
    }

    /** RFC 4648 Base32 decoder. */
    private byte[] base32Decode(String base32) {
        final String ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
        base32 = base32.toUpperCase().replaceAll("[=\\s]", "");
        byte[] result = new byte[base32.length() * 5 / 8];
        int buffer = 0, bitsLeft = 0, index = 0;
        for (char c : base32.toCharArray()) {
            int val = ALPHABET.indexOf(c);
            if (val < 0) throw new IllegalArgumentException("Invalid Base32 char: " + c);
            buffer = (buffer << 5) | val;
            bitsLeft += 5;
            if (bitsLeft >= 8) {
                result[index++] = (byte) ((buffer >> (bitsLeft - 8)) & 0xff);
                bitsLeft -= 8;
            }
        }
        return result;
    }
}
