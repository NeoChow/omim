package com.mapswithme.maps.promo;

import android.support.annotation.MainThread;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import com.mapswithme.util.NetworkPolicy;
import com.mapswithme.util.concurrency.UiThread;

public enum Promo
{
  INSTANCE;

  public interface Listener
  {
    void onCityGalleryReceived(@NonNull PromoCityGallery gallery);
    void onErrorReceived();
  }

  @Nullable
  private Promo.Listener mListener;

  public void setListener(@Nullable Promo.Listener listener)
  {
    mListener = listener;
  }

  // Called from JNI.
  @SuppressWarnings("unused")
  @MainThread
  void onCityGalleryReceived(@NonNull PromoCityGallery gallery)
  {
    if (!UiThread.isUiThread())
      throw new AssertionError("Must be called from UI thread!");

    if (mListener != null)
      mListener.onCityGalleryReceived(gallery);
  }

  // Called from JNI.
  @SuppressWarnings("unused")
  @MainThread
  void onErrorReceived()
  {
    if (!UiThread.isUiThread())
      throw new AssertionError("Must be called from UI thread!");

    if (mListener != null)
      mListener.onErrorReceived();
  }

  public native void nativeRequestCityGallery(@NonNull NetworkPolicy policy, @NonNull String id);
}
