package org.acoustixaudio.opiqo.multi;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

// Instances of this class are fragments representing a single
// object in the collection.
public class ObjectFragment extends Fragment {
    public static final String ARG_OBJECT = "object";
    public MainActivity mainActivity;
    TextView add = null;
    View root = null;
    int position;

    public ObjectFragment(MainActivity _mainActivity) {
        mainActivity = _mainActivity;
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.plugin, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        Bundle args = getArguments();
        ((TextView) view.findViewById(R.id.text1))
                .setText(Integer.toString(args.getInt(ARG_OBJECT)));
        add = view.findViewById(R.id.add);
        position = args.getInt(ARG_OBJECT);
        add.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                addPluginDialog();
            }
        });

        root = view.findViewById(R.id.plugin_box);
    }

    void addPluginDialog () {
        mainActivity.showAddPluginDialog(root, add, position);
    }
}
