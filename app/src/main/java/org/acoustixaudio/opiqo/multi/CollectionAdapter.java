package org.acoustixaudio.opiqo.multi;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.viewpager2.adapter.FragmentStateAdapter;
import androidx.viewpager2.widget.ViewPager2;

import org.acoustixaudio.opiqo.multi.R;

public class CollectionAdapter extends FragmentStateAdapter {
    public MainActivity mainActivity;
    public CollectionAdapter(Fragment fragment) {
        super(fragment);
    }

    public CollectionAdapter(CollectionFragment fragment, MainActivity _mainActivity) {
        super(fragment);
        mainActivity = _mainActivity;
    }

    @NonNull
    @Override
    public Fragment createFragment(int position) {
        // Return a NEW fragment instance in createFragment(int).
        Fragment fragment = new ObjectFragment(mainActivity);
        Bundle args = new Bundle();
        // The object is just an integer.
        args.putInt(ObjectFragment.ARG_OBJECT, position + 1);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public int getItemCount() {
        return 4;
    }
}

