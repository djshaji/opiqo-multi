package org.acoustixaudio.opiqo.multi;

import android.content.Context;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ToggleButton;

import androidx.annotation.NonNull;

import com.google.android.material.slider.Slider;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.lang.reflect.Array;
import java.util.ArrayList;
import java.util.Iterator;

public class UI extends LinearLayout {
    int position;
    JSONObject pluginInfo;
    String pluginName;
    Context context;
    static final String TAG = "UI";
    public View add = null ;

    public UI(Context _context, String _pluginInfo, int _position) {
        super(_context);
        context = _context;
        position = _position;

        LayoutParams params = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        setLayoutParams(params);
        setOrientation(VERTICAL);
        setPadding(20, 20, 20, 20);

        try {
            pluginInfo = new JSONObject(_pluginInfo);
            build();
        } catch (Exception e) {
            e.printStackTrace();
            Toast.makeText(_context, e.getMessage(), Toast.LENGTH_SHORT).show();
        }
    }


    void build () throws JSONException {
        try {
            JSONArray ports = pluginInfo.getJSONArray("port");
            Log.d(TAG, "build: ports " + ports);
            TextView title = new TextView(context);
            pluginName = pluginInfo.getString("name");
            title.setText(pluginName);
            title.setTextSize(22);
            title.setPadding(10, 10, 10, 40);

            LinearLayout header = new LinearLayout(context);
            header.setBackground(getResources().getDrawable(R.drawable.trans2));
            header.setOrientation(HORIZONTAL);
            header.setGravity(Gravity.CENTER_VERTICAL);
            header.addView(title);

            LayoutParams headerParams = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
            headerParams.weight = 1;
            title.setLayoutParams(headerParams);
            Switch bypass = new Switch(context);
            bypass.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                @Override
                public void onCheckedChanged(@NonNull CompoundButton compoundButton, boolean b) {
                    AudioEngine.setPluginEnabled(position, b);
                }
            });

            bypass.setChecked(true);
            addView(header);
            header.addView(bypass);

            if (pluginInfo.has("writableParams")) {
                JSONObject writableParams = pluginInfo.getJSONObject("writableParams");
                Iterator<String> keys = writableParams.keys();
                while (keys.hasNext()) {
                    String key = keys.next();
                    JSONObject value = writableParams.getJSONObject(key);

                    LinearLayout layout = new LinearLayout(context);
                    layout.setGravity(Gravity.CENTER_HORIZONTAL);
                    layout.setOrientation(HORIZONTAL);
                    TextView label = new TextView(context);
                    label.setText(value.getString("label") + ":");
                    label.setPadding(0, 0, 20, 20);
                    layout.addView(label);
                    
                    String range = value.getString("range");
                    String type = value.getString("type");
                    String controlName = value.getString("label");
                    Button sw = new Button(context);

                    sw.setCompoundDrawables(getResources().getDrawable(R.drawable.outline_file_open_24), null, null, null);
                    sw.setText("Choose file");
                    sw.setOnClickListener(new OnClickListener() {
                        @Override
                        public void onClick(View view) {
                            Log.d(TAG, "onClick: do something with " + key + " range " + range + " plugin " + pluginName + " control " + controlName);
                            ((MainActivity) context).chooseFile (position, pluginName, controlName, key, range, type);
                        }
                    });
                    
                    layout.addView(sw);
                    addView(layout);

                    String filesDir = String.format ("%s/user/%s/%s", context.getFilesDir(), pluginName, controlName);

                    File dir = new File(filesDir);
                    if (dir.exists()) {
                        File[] files = dir.listFiles();
                        ArrayList <String> fileNames = new ArrayList<>();
                        if (files != null && files.length > 0) {
                            for (int i = 0; i < files.length; i++) {
                                File file = files[i];
                                if (file.isFile()) {
                                    fileNames.add(file.getName());
                                }
                            }
                        }

                        // add drop down to select from previously chosen files
                        Spinner spinner = new Spinner(context);
                        ArrayAdapter<String> adapter = new ArrayAdapter<>(
                                getContext(),
                                android.R.layout.simple_spinner_item,
                                fileNames
                        );

                        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
                        spinner.setAdapter(adapter);
                        TextView left = new TextView(context), right = new TextView(context);
                        left.setText("<");
                        right.setText(">");

                        left.setPadding(20,10,20,10);
                        right.setPadding(20,10,20,10);

                        left.setBackgroundColor(getResources().getColor(com.google.android.material.R.color.material_dynamic_primary80));
                        right.setBackgroundColor(getResources().getColor(com.google.android.material.R.color.material_dynamic_primary80));

                        LinearLayout layout2 = new LinearLayout(context);
                        layout2.setPadding(0,10,0,10);
                        layout2.setBackgroundColor(getResources().getColor(com.google.android.material.R.color.material_dynamic_neutral90));
                        layout2.setGravity(Gravity.CENTER_VERTICAL);
                        LayoutParams layoutParams = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
                        layoutParams.setMargins(0, 20, 0, 10);
                        layout2.setOrientation(HORIZONTAL);
                        layout2.setLayoutParams(layoutParams);
                        layout2.addView(left);
                        layout2.addView(spinner);
                        layout2.addView(right);
                        addView(layout2);

                        LinearLayout.LayoutParams spinnerParams = new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT);
                        spinnerParams.weight = 1;
                        spinner.setLayoutParams(spinnerParams);

                    }
                }
            }

            for (int i = 0; i < ports.length(); i++) {
                JSONObject port = ports.getJSONObject(i);
                Log.d(TAG, "build: port " + port);

                if (! port.has("type") || port.getString("type").equals("audio"))
                    continue;

                if (port.getString("type").equals("control")) {
                    Slider slider = new Slider(context);
                    slider.setValueFrom((float) port.getDouble("min"));
                    slider.setValueTo((float) port.getDouble("max"));
                    slider.setValue((float) port.getDouble("default"));
                    slider.setLabelFormatter(value -> String.format("%.2f", value));
                    slider.addOnChangeListener((s, value, fromUser) -> {
                        if (fromUser) {
                            try {
                                AudioEngine.setValue(position, port.getInt("index"), value);
                            } catch (JSONException e) {
                                Toast.makeText(context, e.getMessage(), Toast.LENGTH_SHORT).show();
                                throw new RuntimeException(e);
                            }
                        }
                    });

                    TextView label = new TextView(context);
                    label.setText(port.getString("name"));
//                label.setTextSize(16);
                    label.setPadding(0, 0, 0, 20);

                    addView(slider);
                    addView(label);
                } else if (port.getString("type").equals("toggled")) {
                    ToggleButton sw = new ToggleButton(context);
                    sw.setTextOn("ON");
                    sw.setChecked(port.getInt("default") == 1);
                    sw.setTextOff("OFF");
                    sw.setOnCheckedChangeListener((buttonView, isChecked) -> {
                        try {
                            AudioEngine.setValue(position, port.getInt("index"), isChecked ? 1.0f : 0.0f);
                        } catch (JSONException e) {
                            Toast.makeText(context, e.getMessage(), Toast.LENGTH_SHORT).show();
                            throw new RuntimeException(e);
                        }
                    });

                    LinearLayout layout = new LinearLayout(context);
                    layout.setGravity(Gravity.CENTER_HORIZONTAL);
                    layout.setOrientation(HORIZONTAL);
                    TextView label = new TextView(context);
                    label.setText(port.getString("name") + ":");
                    label.setPadding(0, 0, 20, 20);
                    layout.addView(label);
                    layout.addView(sw);
                    addView(layout);
                } else if (port.getString("type").equals("dropdown")) {
                    JSONArray options = port.getJSONArray("options");
                    ArrayList <String> optionNames = new ArrayList<>();
                    ArrayList <Integer> optionValues = new ArrayList<>();
                    for (int j = 0; j < options.length(); j++) {
                        optionNames.add(options.getJSONObject(j).getString("label"));
                        optionValues.add(options.getJSONObject(j).getInt("value"));
                    }

                    Spinner spinner = new Spinner(context);
                    ArrayAdapter<String> adapter = new ArrayAdapter<>(
                            getContext(),
                            android.R.layout.simple_spinner_item,
                            optionNames
                    );

                    adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
                    spinner.setAdapter(adapter);
                    int defaultIndex = optionValues.indexOf(port.getInt("default"));
                    if (defaultIndex != -1)
                        spinner.setSelection(defaultIndex);

                    spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                                                          @Override
                                                          public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                                                              try {
                                                                  AudioEngine.setValue(UI.this.position, port.getInt("index"), optionValues.get(position));
                                                              } catch (JSONException e) {
                                                                  Toast.makeText(context, e.getMessage(), Toast.LENGTH_SHORT).show();
                                                                  throw new RuntimeException(e);
                                                              }
                                                          }

                                                          @Override
                                                          public void onNothingSelected(AdapterView<?> adapterView) {
//                            AudioEngine.setValue(position, port.getInt("index"), optionValues.get(i));
                                                          }
                                                      });


                    TextView left = new TextView(context), right = new TextView(context);
                    left.setText("<");
                    right.setText(">");

                    left.setPadding(20,10,20,10);
                    right.setPadding(20,10,20,10);

                    left.setBackgroundColor(getResources().getColor(com.google.android.material.R.color.material_dynamic_primary80));
                    right.setBackgroundColor(getResources().getColor(com.google.android.material.R.color.material_dynamic_primary80));

                    left.setOnClickListener(new OnClickListener() {
                        @Override
                        public void onClick(View view) {
                            int max = optionNames.size();
                            int current = spinner.getSelectedItemPosition();
                            spinner.setSelection((current - 1 + max) % max);
                        }
                    });

                    right.setOnClickListener(new OnClickListener() {
                        @Override
                        public void onClick(View view) {
                            int max = optionNames.size();
                            int current = spinner.getSelectedItemPosition();
                            spinner.setSelection((current + 1) % max);
                        }
                    });

                    LinearLayout layout = new LinearLayout(context);
                    layout.setPadding(0,10,0,10);
                    layout.setBackgroundColor(getResources().getColor(com.google.android.material.R.color.material_dynamic_neutral90));
                    layout.setGravity(Gravity.CENTER_VERTICAL);
                    LayoutParams layoutParams = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
                    layoutParams.setMargins(0, 20, 0, 10);
                    layout.setOrientation(HORIZONTAL);
                    layout.setLayoutParams(layoutParams);
                    TextView label = new TextView(context);
                    label.setText(port.getString("name") + ":");
                    label.setPadding(0, 0, 20, 0);
                    layout.addView(label);
                    layout.addView(left);
                    layout.addView(spinner);
                    layout.addView(right);
                    addView(layout);

                    LinearLayout.LayoutParams spinnerParams = new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT);
                    spinnerParams.weight = 1;
                    spinner.setLayoutParams(spinnerParams);
                }

                /*
                else if (port.getString("type").equals("atom")) {
                    ImageButton imageButton = new ImageButton(context);
                    imageButton.setImageResource(R.drawable.outline_file_open_24);

                    LinearLayout layout = new LinearLayout(context);
                    layout.setGravity(Gravity.CENTER_HORIZONTAL);
                    layout.setOrientation(HORIZONTAL);
                    TextView label = new TextView(context);
                    label.setText(port.getString("name") + ":");
                    label.setPadding(0, 0, 20, 20);
                    layout.addView(label);
                    layout.addView(imageButton);
                    addView(layout);
                }

                 */
            }
        } catch (Exception e) {
            e.printStackTrace();
            Toast.makeText(context, e.getMessage(), Toast.LENGTH_SHORT).show();
        }

        Button del = new Button(context);
        LayoutParams params = new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        params.setMargins(0, 40, 0, 0);
        params.gravity = Gravity.END;
        del.setBackgroundColor(context.getResources().getColor(R.color.material_red700));
        del.setTextColor(context.getResources().getColor(R.color.white));
        del.setLayoutParams(params);
        del.setText("Delete");
        del.setOnClickListener(v -> {
            AudioEngine.deletePlugin(position);
            if (add != null)
                 add.setVisibility(View.VISIBLE);

            ((LinearLayout) getParent()).removeView(this);
        });

        addView(del);
    }
}
