package org.acoustixaudio.opiqo.multi;

import android.content.Context;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;

import com.google.android.material.slider.Slider;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.Iterator;

public class UI extends LinearLayout {
    int position;
    JSONObject pluginInfo;
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
            title.setText(pluginInfo.getString("name"));
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

            for (int i = 0; i < ports.length(); i++) {
                JSONObject port = ports.getJSONObject(i);
                Log.d(TAG, "build: port " + port);

                if (! port.has("type") || ! port.getString("type").equals("control"))
                    continue;

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
