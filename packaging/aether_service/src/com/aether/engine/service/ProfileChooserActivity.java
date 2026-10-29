package com.aether.engine.service;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

/**
 * AETHER Engine - Interactive Performance Profile Chooser
 * Launched on Quick Settings Tile long-press.
 * Displays 3 AMOLED cards: Battery Saver, Balanced, Gaming Turbo.
 */
public class ProfileChooserActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestWindowFeature(Window.FEATURE_NO_TITLE);

        int currentMode = AetherIpcHelper.getProfileMode();

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(22), dp(22), dp(22), dp(22));

        GradientDrawable bg = new GradientDrawable();
        bg.setColor(Color.parseColor("#0F131C")); // Pure AMOLED Dark
        bg.setCornerRadius(dp(24));
        bg.setStroke(dp(1.5f), Color.parseColor("#00D4FF")); // Neon Cyan border
        root.setBackground(bg);

        // Header Title
        TextView title = new TextView(this);
        title.setText("⚡ Performance Mode");
        title.setTextColor(Color.parseColor("#FFFFFF"));
        title.setTextSize(18f);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setGravity(Gravity.CENTER_HORIZONTAL);
        root.addView(title);

        TextView sub = new TextView(this);
        sub.setText("Select active scheduler profile");
        sub.setTextColor(Color.parseColor("#94A3B8"));
        sub.setTextSize(12f);
        sub.setGravity(Gravity.CENTER_HORIZONTAL);
        LinearLayout.LayoutParams subLp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        subLp.setMargins(0, dp(4), 0, dp(16));
        root.addView(sub, subLp);

        // 3 Cards: Battery Saver (1), Balanced (2), Gaming Turbo (3)
        root.addView(createProfileCard("🔋 Battery Saver", "Underclocks CPU/GPU • Max Battery Life", 1, currentMode == 1, "#10B981"));
        root.addView(createProfileCard("⚖️ Balanced (Stock)", "Dynamic Energy-Aware Scheduling", 2, currentMode == 2, "#00D4FF"));
        root.addView(createProfileCard("🚀 Gaming Turbo", "Unconstrained Clocks • Zero Stutter", 3, currentMode == 3, "#F59E0B"));

        setContentView(root);
    }

    private View createProfileCard(final String titleText, String descText, final int mode, boolean isActive, String accentColor) {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(16), dp(12), dp(16), dp(12));

        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.setMargins(0, dp(6), 0, dp(6));
        card.setLayoutParams(lp);

        GradientDrawable cardBg = new GradientDrawable();
        cardBg.setColor(Color.parseColor(isActive ? "#1A2234" : "#141824"));
        cardBg.setCornerRadius(dp(16));
        if (isActive) {
            cardBg.setStroke(dp(2), Color.parseColor(accentColor));
        } else {
            cardBg.setStroke(dp(1), Color.parseColor("#252D3D"));
        }
        card.setBackground(cardBg);

        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);

        TextView title = new TextView(this);
        title.setText(titleText);
        title.setTextColor(Color.parseColor(isActive ? accentColor : "#FFFFFF"));
        title.setTextSize(15f);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        LinearLayout.LayoutParams titleLp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        row.addView(title, titleLp);

        if (isActive) {
            TextView tag = new TextView(this);
            tag.setText("ACTIVE");
            tag.setTextColor(Color.parseColor(accentColor));
            tag.setTextSize(11f);
            tag.setTypeface(Typeface.MONOSPACE, Typeface.BOLD);
            row.addView(tag);
        }
        card.addView(row);

        TextView desc = new TextView(this);
        desc.setText(descText);
        desc.setTextColor(Color.parseColor("#94A3B8"));
        desc.setTextSize(12f);
        desc.setPadding(0, dp(3), 0, 0);
        card.addView(desc);

        card.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                AetherIpcHelper.setProfileMode(mode);
                sendBroadcast(new Intent("com.aether.engine.PROFILE_CHANGED"));
                Toast.makeText(ProfileChooserActivity.this, "Profile: " + titleText, Toast.LENGTH_SHORT).show();
                finish();
            }
        });

        return card;
    }

    private int dp(float val) {
        return (int) (val * getResources().getDisplayMetrics().density);
    }
}
