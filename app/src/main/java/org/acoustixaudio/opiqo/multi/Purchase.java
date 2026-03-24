package org.acoustixaudio.opiqo.multi;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Log;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import com.android.billingclient.api.AcknowledgePurchaseParams;
import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.BillingClientStateListener;
import com.android.billingclient.api.BillingFlowParams;
import com.android.billingclient.api.BillingResult;
import com.android.billingclient.api.ProductDetails;
import com.android.billingclient.api.PurchasesUpdatedListener;
import com.android.billingclient.api.QueryProductDetailsParams;
import com.android.billingclient.api.QueryPurchasesParams;

import java.util.Collections;

public class Purchase extends AppCompatActivity {

    private static final String TAG = "Purchase";
    private static final String PRODUCT_ID = "org.acoustixaudio.opiqo.multi.pro";

    private BillingClient billingClient;
    private ProductDetails proProductDetails;

    private Button purchaseButton;
    private TextView priceText;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_purchase);
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });

        purchaseButton = findViewById(R.id.btn_purchase);
        priceText = findViewById(R.id.tv_price);

        purchaseButton.setEnabled(false);
        purchaseButton.setOnClickListener(v -> launchPurchaseFlow());
        findViewById(R.id.btn_restore).setOnClickListener(v -> queryExistingPurchases());

        setupBillingClient();
    }

    private void setupBillingClient() {
        PurchasesUpdatedListener purchasesUpdatedListener = (billingResult, purchases) -> {
            if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.OK
                    && purchases != null) {
                for (com.android.billingclient.api.Purchase purchase : purchases) {
                    handlePurchase(purchase);
                }
            } else if (billingResult.getResponseCode() != BillingClient.BillingResponseCode.USER_CANCELED) {
                Log.e(TAG, "Purchase error: " + billingResult.getDebugMessage());
                runOnUiThread(() -> Toast.makeText(this,
                        "Purchase failed: " + billingResult.getDebugMessage(),
                        Toast.LENGTH_LONG).show());
            }
        };

        billingClient = BillingClient.newBuilder(this)
                .setListener(purchasesUpdatedListener)
                .enablePendingPurchases()
                .build();

        billingClient.startConnection(new BillingClientStateListener() {
            @Override
            public void onBillingSetupFinished(@NonNull BillingResult billingResult) {
                if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.OK) {
                    queryProductDetails();
                    queryExistingPurchases();
                } else {
                    Log.e(TAG, "Billing setup failed: " + billingResult.getDebugMessage());
                    runOnUiThread(() -> Toast.makeText(Purchase.this,
                            "Billing unavailable", Toast.LENGTH_LONG).show());
                }
            }

            @Override
            public void onBillingServiceDisconnected() {
                Log.w(TAG, "Billing service disconnected");
            }
        });
    }

    private void queryProductDetails() {
        QueryProductDetailsParams params = QueryProductDetailsParams.newBuilder()
                .setProductList(Collections.singletonList(
                        QueryProductDetailsParams.Product.newBuilder()
                                .setProductId(PRODUCT_ID)
                                .setProductType(BillingClient.ProductType.INAPP)
                                .build()
                ))
                .build();

        billingClient.queryProductDetailsAsync(params, (billingResult, productDetailsList) -> {
            if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.OK
                    && !productDetailsList.isEmpty()) {
                proProductDetails = productDetailsList.get(0);
                runOnUiThread(() -> {
                    ProductDetails.OneTimePurchaseOfferDetails offer =
                            proProductDetails.getOneTimePurchaseOfferDetails();
                    if (offer != null) {
                        priceText.setText(offer.getFormattedPrice());
                    }
                    purchaseButton.setEnabled(true);
                });
            } else {
                Log.e(TAG, "Failed to get product details: " + billingResult.getDebugMessage());
            }
        });
    }

    private void launchPurchaseFlow() {
        if (proProductDetails == null) return;

        BillingFlowParams billingFlowParams = BillingFlowParams.newBuilder()
                .setProductDetailsParamsList(Collections.singletonList(
                        BillingFlowParams.ProductDetailsParams.newBuilder()
                                .setProductDetails(proProductDetails)
                                .build()
                ))
                .build();

        billingClient.launchBillingFlow(this, billingFlowParams);
    }

    private void handlePurchase(com.android.billingclient.api.Purchase purchase) {
        if (purchase.getPurchaseState() != com.android.billingclient.api.Purchase.PurchaseState.PURCHASED) {
            return;
        }
        if (!purchase.isAcknowledged()) {
            AcknowledgePurchaseParams params = AcknowledgePurchaseParams.newBuilder()
                    .setPurchaseToken(purchase.getPurchaseToken())
                    .build();
            billingClient.acknowledgePurchase(params, billingResult -> {
                if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.OK) {
                    grantProAccess();
                }
            });
        } else {
            grantProAccess();
        }
    }

    private void queryExistingPurchases() {
        billingClient.queryPurchasesAsync(
                QueryPurchasesParams.newBuilder()
                        .setProductType(BillingClient.ProductType.INAPP)
                        .build(),
                (billingResult, purchaseList) -> {
                    if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.OK) {
                        for (com.android.billingclient.api.Purchase purchase : purchaseList) {
                            handlePurchase(purchase);
                        }
                    }
                });
    }

    private void grantProAccess() {
        SharedPreferences prefs = getSharedPreferences("core", MODE_PRIVATE);
        prefs.edit().putBoolean("is_pro", true).apply();
        runOnUiThread(() -> {
            Toast.makeText(this, "Pro upgrade activated!", Toast.LENGTH_LONG).show();
            setResult(RESULT_OK);
            MainActivity.proVersion = true;
            finish();
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (billingClient != null && billingClient.isReady()) {
            billingClient.endConnection();
        }
    }
}