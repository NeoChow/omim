package com.mapswithme.maps;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.location.Location;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.CallSuper;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.StyleRes;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.support.v7.app.AlertDialog;
import android.support.v7.widget.Toolbar;
import android.text.TextUtils;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.WindowManager;
import android.widget.ImageButton;
import android.widget.Toast;

import com.mapswithme.maps.Framework.MapObjectListener;
import com.mapswithme.maps.activity.CustomNavigateUpListener;
import com.mapswithme.maps.ads.LikesManager;
import com.mapswithme.maps.api.ParsedMwmRequest;
import com.mapswithme.maps.auth.PassportAuthDialogFragment;
import com.mapswithme.maps.background.NotificationCandidate;
import com.mapswithme.maps.background.Notifier;
import com.mapswithme.maps.base.BaseMwmFragmentActivity;
import com.mapswithme.maps.base.OnBackPressListener;
import com.mapswithme.maps.bookmarks.BookmarkCategoriesActivity;
import com.mapswithme.maps.bookmarks.BookmarksCatalogActivity;
import com.mapswithme.maps.bookmarks.data.BookmarkCategory;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.maps.bookmarks.data.CatalogCustomProperty;
import com.mapswithme.maps.bookmarks.data.CatalogTagsGroup;
import com.mapswithme.maps.bookmarks.data.MapObject;
import com.mapswithme.maps.dialog.AlertDialogCallback;
import com.mapswithme.maps.dialog.DialogUtils;
import com.mapswithme.maps.dialog.DrivingOptionsDialogFactory;
import com.mapswithme.maps.discovery.DiscoveryActivity;
import com.mapswithme.maps.discovery.DiscoveryFragment;
import com.mapswithme.maps.discovery.ItemType;
import com.mapswithme.maps.downloader.DownloaderActivity;
import com.mapswithme.maps.downloader.DownloaderFragment;
import com.mapswithme.maps.downloader.MapManager;
import com.mapswithme.maps.downloader.MigrationFragment;
import com.mapswithme.maps.downloader.OnmapDownloader;
import com.mapswithme.maps.editor.Editor;
import com.mapswithme.maps.editor.EditorActivity;
import com.mapswithme.maps.editor.EditorHostFragment;
import com.mapswithme.maps.editor.FeatureCategoryActivity;
import com.mapswithme.maps.editor.ReportFragment;
import com.mapswithme.maps.gallery.Items;
import com.mapswithme.maps.intent.Factory;
import com.mapswithme.maps.intent.MapTask;
import com.mapswithme.maps.location.CompassData;
import com.mapswithme.maps.location.LocationHelper;
import com.mapswithme.maps.maplayer.MapLayerCompositeController;
import com.mapswithme.maps.maplayer.Mode;
import com.mapswithme.maps.maplayer.subway.OnSubwayLayerToggleListener;
import com.mapswithme.maps.maplayer.subway.SubwayManager;
import com.mapswithme.maps.maplayer.traffic.OnTrafficLayerToggleListener;
import com.mapswithme.maps.maplayer.traffic.TrafficManager;
import com.mapswithme.maps.maplayer.traffic.widget.TrafficButton;
import com.mapswithme.maps.news.IntroductionDialogFragment;
import com.mapswithme.maps.news.IntroductionScreenFactory;
import com.mapswithme.maps.purchase.AdsRemovalActivationCallback;
import com.mapswithme.maps.purchase.AdsRemovalPurchaseControllerProvider;
import com.mapswithme.maps.purchase.FailedPurchaseChecker;
import com.mapswithme.maps.purchase.PurchaseCallback;
import com.mapswithme.maps.purchase.PurchaseController;
import com.mapswithme.maps.purchase.PurchaseFactory;
import com.mapswithme.maps.routing.NavigationController;
import com.mapswithme.maps.routing.RoutePointInfo;
import com.mapswithme.maps.routing.RoutingBottomMenuListener;
import com.mapswithme.maps.routing.RoutingController;
import com.mapswithme.maps.routing.RoutingErrorDialogFragment;
import com.mapswithme.maps.routing.RoutingOptions;
import com.mapswithme.maps.routing.RoutingPlanFragment;
import com.mapswithme.maps.routing.RoutingPlanInplaceController;
import com.mapswithme.maps.search.BookingFilterParams;
import com.mapswithme.maps.search.FilterActivity;
import com.mapswithme.maps.search.FloatingSearchToolbarController;
import com.mapswithme.maps.search.HotelsFilter;
import com.mapswithme.maps.search.NativeSearchListener;
import com.mapswithme.maps.search.SearchActivity;
import com.mapswithme.maps.search.SearchEngine;
import com.mapswithme.maps.search.SearchFilterController;
import com.mapswithme.maps.search.SearchFragment;
import com.mapswithme.maps.search.SearchResult;
import com.mapswithme.maps.settings.DrivingOptionsActivity;
import com.mapswithme.maps.settings.RoadType;
import com.mapswithme.maps.settings.SettingsActivity;
import com.mapswithme.maps.settings.StoragePathManager;
import com.mapswithme.maps.settings.UnitLocale;
import com.mapswithme.maps.sound.TtsPlayer;
import com.mapswithme.maps.taxi.TaxiInfo;
import com.mapswithme.maps.taxi.TaxiManager;
import com.mapswithme.maps.tips.TipsApi;
import com.mapswithme.maps.widget.FadeView;
import com.mapswithme.maps.widget.menu.BaseMenu;
import com.mapswithme.maps.widget.menu.MainMenu;
import com.mapswithme.maps.widget.menu.MyPositionButton;
import com.mapswithme.maps.widget.placepage.BottomSheetPlacePageController;
import com.mapswithme.maps.widget.placepage.PlacePageController;
import com.mapswithme.maps.widget.placepage.RoutingModeListener;
import com.mapswithme.util.Counters;
import com.mapswithme.util.InputUtils;
import com.mapswithme.util.PermissionsUtils;
import com.mapswithme.util.ThemeSwitcher;
import com.mapswithme.util.ThemeUtils;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.Utils;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;
import com.mapswithme.util.permissions.PermissionsResult;
import com.mapswithme.util.sharing.ShareOption;
import com.mapswithme.util.sharing.SharingHelper;
import com.mapswithme.util.sharing.TargetUtils;
import com.mapswithme.util.statistics.AlohaHelper;
import com.mapswithme.util.statistics.Statistics;

import java.util.List;
import java.util.Stack;

public class MwmActivity extends BaseMwmFragmentActivity
                      implements MapObjectListener,
                                 View.OnTouchListener,
                                 OnClickListener,
                                 MapRenderingListener,
                                 CustomNavigateUpListener,
                                 RoutingController.Container,
                                 LocationHelper.UiCallback,
                                 FloatingSearchToolbarController.VisibilityListener,
                                 NativeSearchListener,
                                 NavigationButtonsAnimationController.OnTranslationChangedListener,
                                 RoutingPlanInplaceController.RoutingPlanListener,
                                 RoutingBottomMenuListener,
                                 BookmarkManager.BookmarksLoadingListener,
                                 DiscoveryFragment.DiscoveryListener,
                                 FloatingSearchToolbarController.SearchToolbarListener,
                                 OnTrafficLayerToggleListener,
                                 OnSubwayLayerToggleListener,
                                 BookmarkManager.BookmarksCatalogListener,
                                 AdsRemovalPurchaseControllerProvider,
                                 AdsRemovalActivationCallback,
                                 PlacePageController.SlideListener,
                                 AlertDialogCallback, RoutingModeListener
{
  private static final Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.MISC);
  private static final String TAG = MwmActivity.class.getSimpleName();

  public static final String EXTRA_TASK = "map_task";
  public static final String EXTRA_LAUNCH_BY_DEEP_LINK = "launch_by_deep_link";
  private static final String EXTRA_CONSUMED = "mwm.extra.intent.processed";

  private static final String[] DOCKED_FRAGMENTS = { SearchFragment.class.getName(),
                                                     DownloaderFragment.class.getName(),
                                                     MigrationFragment.class.getName(),
                                                     RoutingPlanFragment.class.getName(),
                                                     EditorHostFragment.class.getName(),
                                                     ReportFragment.class.getName(),
                                                     DiscoveryFragment.class.getName() };

  private static final String STATE_MAP_OBJECT = "MapObject";
  private static final String EXTRA_LOCATION_DIALOG_IS_ANNOYING = "LOCATION_DIALOG_IS_ANNOYING";

  private static final int REQ_CODE_LOCATION_PERMISSION = 1;
  private static final int REQ_CODE_DISCOVERY = 2;
  private static final int REQ_CODE_SHOW_SIMILAR_HOTELS = 3;
  public static final int REQ_CODE_ERROR_DRIVING_OPTIONS_DIALOG = 5;
  public static final int REQ_CODE_DRIVING_OPTIONS = 6;
  public static final String ERROR_DRIVING_OPTIONS_DIALOG_TAG = "error_driving_options_dialog_tag";

  // Map tasks that we run AFTER rendering initialized
  private final Stack<MapTask> mTasks = new Stack<>();
  private final StoragePathManager mPathManager = new StoragePathManager();

  @Nullable
  private MapFragment mMapFragment;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private FadeView mFadeView;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private View mPositionChooser;

  private RoutingPlanInplaceController mRoutingPlanInplaceController;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private NavigationController mNavigationController;

  private MainMenu mMainMenu;

  private PanelAnimator mPanelAnimator;
  @Nullable
  private OnmapDownloader mOnmapDownloader;

  @Nullable
  private MyPositionButton mNavMyPosition;
  @Nullable
  private NavigationButtonsAnimationController mNavAnimationController;
  @SuppressWarnings("NullableProblems")
  @NonNull
  private MapLayerCompositeController mToggleMapLayerController;
  @Nullable
  private SearchFilterController mFilterController;

  private boolean mIsTabletLayout;
  private boolean mIsFullscreen;
  private boolean mIsFullscreenAnimating;
  private boolean mIsAppearMenuLater;
  private boolean mIsLaunchByDeepLink;

  private FloatingSearchToolbarController mSearchController;

  private boolean mLocationErrorDialogAnnoying = false;
  @Nullable
  private Dialog mLocationErrorDialog;

  private boolean mRestoreRoutingPlanFragmentNeeded;
  @Nullable
  private Bundle mSavedForTabletState;
  @Nullable
  private PurchaseController<PurchaseCallback> mAdsRemovalPurchaseController;
  @Nullable
  private PurchaseController<FailedPurchaseChecker> mBookmarkPurchaseController;
  @NonNull
  private final OnClickListener mOnMyPositionClickListener = new CurrentPositionClickListener();
  @SuppressWarnings("NullableProblems")
  @NonNull
  private PlacePageController mPlacePageController;

  public interface LeftAnimationTrackListener
  {
    void onTrackStarted(boolean collapsed);

    void onTrackFinished(boolean collapsed);

    void onTrackLeftAnimation(float offset);
  }

  public static Intent createShowMapIntent(@NonNull Context context, @Nullable String countryId)
  {
    return new Intent(context, DownloadResourcesLegacyActivity.class)
               .putExtra(DownloadResourcesLegacyActivity.EXTRA_COUNTRY, countryId);
  }

  @NonNull
  public static Intent createAuthenticateIntent(@NonNull Context context)
  {
    return new Intent(context, MwmActivity.class)
        .addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION)
        .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP)
        .putExtra(MwmActivity.EXTRA_TASK,
                  new Factory.ShowDialogTask(PassportAuthDialogFragment.class.getName()));
  }

  @NonNull
  public static Intent createLeaveReviewIntent(@NonNull Context context,
                                               @NonNull NotificationCandidate.UgcReview nc)
  {
    return new Intent(context, MwmActivity.class)
      .addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION)
      .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP)
      .putExtra(MwmActivity.EXTRA_TASK, new Factory.ShowUGCEditorTask(nc));
  }

  @Override
  public void onRenderingCreated()
  {
    checkMeasurementSystem();
    checkKitkatMigrationMove();

    LocationHelper.INSTANCE.attach(this);
  }

  @Override
  public void onRenderingRestored()
  {
    runTasks();
  }

  @Override
  public void onRenderingInitializationFinished()
  {
    runTasks();
  }

  private void myPositionClick()
  {
    mLocationErrorDialogAnnoying = false;
    LocationHelper.INSTANCE.setStopLocationUpdateByUser(false);
    LocationHelper.INSTANCE.switchToNextMode();
    LocationHelper.INSTANCE.restart();
  }

  private void runTasks()
  {
    while (!mTasks.isEmpty())
      mTasks.pop().run(this);
  }

  private static void checkMeasurementSystem()
  {
    UnitLocale.initializeCurrentUnits();
  }

  private void checkKitkatMigrationMove()
  {
    mPathManager.checkKitkatMigration(this);
  }

  @Override
  protected int getFragmentContentResId()
  {
    return (mIsTabletLayout ? R.id.fragment_container
                            : super.getFragmentContentResId());
  }

  @Nullable
  Fragment getFragment(Class<? extends Fragment> clazz)
  {
    if (!mIsTabletLayout)
      throw new IllegalStateException("Must be called for tablets only!");

    return getSupportFragmentManager().findFragmentByTag(clazz.getName());
  }

  void replaceFragmentInternal(Class<? extends Fragment> fragmentClass, Bundle args)
  {
    super.replaceFragment(fragmentClass, args, null);
  }

  @Override
  public void replaceFragment(@NonNull Class<? extends Fragment> fragmentClass, @Nullable Bundle args, @Nullable Runnable completionListener)
  {
    if (mPanelAnimator.isVisible() && getFragment(fragmentClass) != null)
    {
      if (completionListener != null)
        completionListener.run();
      return;
    }

    mPanelAnimator.show(fragmentClass, args, completionListener);
  }

  public boolean containsFragment(@NonNull Class<? extends Fragment> fragmentClass)
  {
    return mIsTabletLayout && getFragment(fragmentClass) != null;
  }

  private void showBookmarks()
  {
    BookmarkCategoriesActivity.startForResult(this);
  }

  private void showTabletSearch(@Nullable Intent data, @NonNull String query)
  {
    if (mFilterController == null || data == null)
      return;

    BookingFilterParams params = data.getParcelableExtra(FilterActivity.EXTRA_FILTER_PARAMS);
    HotelsFilter filter = data.getParcelableExtra(FilterActivity.EXTRA_FILTER);
    mFilterController.setFilterAndParams(filter, params);

    showSearch(query);
  }

  public void showSearch(String query)
  {
    if (mIsTabletLayout)
    {
      mSearchController.hide();

      final Bundle args = new Bundle();
      args.putString(SearchActivity.EXTRA_QUERY, query);
      if (mFilterController != null)
      {
        args.putParcelable(FilterActivity.EXTRA_FILTER, mFilterController.getFilter());
        args.putParcelable(FilterActivity.EXTRA_FILTER_PARAMS, mFilterController.getBookingFilterParams());
      }
      replaceFragment(SearchFragment.class, args, null);
    }
    else
    {
      HotelsFilter filter = null;
      BookingFilterParams params = null;
      if (mFilterController != null)
      {
        filter = mFilterController.getFilter();
        params = mFilterController.getBookingFilterParams();
      }
      SearchActivity.start(this, query, filter, params);
    }
    if (mFilterController != null)
      mFilterController.resetFilter();
  }

  public void showEditor()
  {
    // TODO(yunikkk) think about refactoring. It probably should be called in editor.
    Editor.nativeStartEdit();
    Statistics.INSTANCE.trackEditorLaunch(false);
    if (mIsTabletLayout)
      replaceFragment(EditorHostFragment.class, null, null);
    else
      EditorActivity.start(this);
  }

  private void shareMyLocation()
  {
    final Location loc = LocationHelper.INSTANCE.getSavedLocation();
    if (loc != null)
    {
      final String geoUrl = Framework.nativeGetGe0Url(loc.getLatitude(), loc.getLongitude(), Framework.nativeGetDrawScale(), "");
      final String httpUrl = Framework.getHttpGe0Url(loc.getLatitude(), loc.getLongitude(), Framework.nativeGetDrawScale(), "");
      final String body = getString(R.string.my_position_share_sms, geoUrl, httpUrl);
      ShareOption.ANY.share(this, body);
      return;
    }

    new AlertDialog.Builder(MwmActivity.this)
        .setMessage(R.string.unknown_current_position)
        .setCancelable(true)
        .setPositiveButton(android.R.string.ok, null)
        .show();
  }

  @Override
  public void showDownloader(boolean openDownloaded)
  {
    if (RoutingController.get().checkMigration(this))
      return;

    final Bundle args = new Bundle();
    args.putBoolean(DownloaderActivity.EXTRA_OPEN_DOWNLOADED, openDownloaded);
    if (mIsTabletLayout)
    {
      SearchEngine.INSTANCE.cancel();
      mSearchController.refreshToolbar();
      replaceFragment(MapManager.nativeIsLegacyMode() ? MigrationFragment.class : DownloaderFragment.class, args, null);
    }
    else
    {
      startActivity(new Intent(this, DownloaderActivity.class).putExtras(args));
    }
  }

  @Override
  @StyleRes
  public int getThemeResourceId(@NonNull String theme)
  {
    if (ThemeUtils.isDefaultTheme(theme))
      return R.style.MwmTheme_MainActivity;

    if (ThemeUtils.isNightTheme(theme))
      return R.style.MwmTheme_Night_MainActivity;

    return super.getThemeResourceId(theme);
  }

  @SuppressLint("InlinedApi")
  @CallSuper
  @Override
  protected void onSafeCreate(@Nullable Bundle savedInstanceState)
  {
    super.onSafeCreate(savedInstanceState);
    if (savedInstanceState != null)
      mLocationErrorDialogAnnoying = savedInstanceState.getBoolean(EXTRA_LOCATION_DIALOG_IS_ANNOYING);
    mIsTabletLayout = getResources().getBoolean(R.bool.tabletLayout);

    if (!mIsTabletLayout && (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP))
      getWindow().addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);

    setContentView(R.layout.activity_map);
    mPlacePageController = new BottomSheetPlacePageController(this, this, this,
                                                              this);
    mPlacePageController.initialize();

    mIsLaunchByDeepLink = getIntent().getBooleanExtra(EXTRA_LAUNCH_BY_DEEP_LINK, false);
    initViews();

    Statistics.INSTANCE.trackConnectionState();

    mSearchController = new FloatingSearchToolbarController(this, this);
    mSearchController.setVisibilityListener(this);

    SharingHelper.INSTANCE.initialize();

    mAdsRemovalPurchaseController = PurchaseFactory.createAdsRemovalPurchaseController(this);
    mAdsRemovalPurchaseController.initialize(this);
    mAdsRemovalPurchaseController.validateExistingPurchases();

    mBookmarkPurchaseController = PurchaseFactory.createFailedBookmarkPurchaseController(this);
    mBookmarkPurchaseController.initialize(this);
    mBookmarkPurchaseController.validateExistingPurchases();

    boolean isConsumed = savedInstanceState == null && processIntent(getIntent());
    // If the map activity is launched by any incoming intent (deeplink, update maps event, etc)
    // or it's the first launch (onboarding) we haven't to try restoring the route,
    // showing the tips, etc.
    if (isConsumed || MwmApplication.from(this).isFirstLaunch())
      return;

    if (savedInstanceState == null && RoutingController.get().hasSavedRoute())
    {
      addTask(new Factory.RestoreRouteTask());
      return;
    }

    initTips();
  }

  private void initViews()
  {
    initMap();
    initNavigationButtons();

    if (!mIsTabletLayout)
    {
      mRoutingPlanInplaceController = new RoutingPlanInplaceController(this, this, this);
      removeCurrentFragment(false);
    }

    mNavigationController = new NavigationController(this);
    TrafficManager.INSTANCE.attach(mNavigationController);

    initMainMenu();
    initOnmapDownloader();
    initPositionChooser();
    initFilterViews();
  }

  private void initTips()
  {
    TipsApi api = TipsApi.requestCurrent(this, getClass());
    if (api == TipsApi.STUB)
      return;

    api.showTutorial(getActivity());

    Statistics.INSTANCE.trackTipsEvent(Statistics.EventName.TIPS_TRICKS_SHOW, api.ordinal());
  }

  private void initFilterViews()
  {
    View frame = findViewById(R.id.filter_frame);
    if (frame != null)
    {
      mFilterController = new SearchFilterController(frame, new SearchFilterController
          .DefaultFilterListener()
      {
        @Override
        public void onShowOnMapClick()
        {
          showSearch(mSearchController.getQuery());
        }

        @Override
        public void onFilterClick()
        {
          HotelsFilter filter = null;
          BookingFilterParams params = null;
          if (mFilterController != null)
          {
            filter = mFilterController.getFilter();
            params = mFilterController.getBookingFilterParams();
          }
          FilterActivity.startForResult(MwmActivity.this, filter, params,
                                        FilterActivity.REQ_CODE_FILTER);
        }

        @Override
        public void onFilterClear()
        {
          runSearch();
        }
      }, R.string.search_in_table);
    }
  }

  private void runSearch()
  {
    // The previous search should be cancelled before the new one is started, since previous search
    // results are no longer needed.
    SearchEngine.INSTANCE.cancel();

    SearchEngine.INSTANCE.searchInteractive(mSearchController.getQuery(), System.nanoTime(),
                                   false /* isMapAndTable */,
                                   mFilterController != null ? mFilterController.getFilter() : null,
                                   mFilterController != null ? mFilterController.getBookingFilterParams() : null);
    SearchEngine.INSTANCE.setQuery(mSearchController.getQuery());
  }

  private void initPositionChooser()
  {
    mPositionChooser = findViewById(R.id.position_chooser);
    if (mPositionChooser == null)
      return;

    final Toolbar toolbar = mPositionChooser.findViewById(R.id.toolbar_position_chooser);
    UiUtils.extendViewWithStatusBar(toolbar);
    UiUtils.showHomeUpButton(toolbar);
    toolbar.setNavigationOnClickListener(v -> hidePositionChooser());
    mPositionChooser.findViewById(R.id.done).setOnClickListener(
        v ->
        {
          Statistics.INSTANCE.trackEditorLaunch(true);
          hidePositionChooser();
          if (Framework.nativeIsDownloadedMapAtScreenCenter())
            startActivity(new Intent(MwmActivity.this, FeatureCategoryActivity.class));
          else
            DialogUtils.showAlertDialog(MwmActivity.this, R.string.message_invalid_feature_position);
        });
    UiUtils.hide(mPositionChooser);
  }

  public void showPositionChooser(boolean isBusiness, boolean applyPosition)
  {
    UiUtils.show(mPositionChooser);
    setFullscreen(true);
    Framework.nativeTurnOnChoosePositionMode(isBusiness, applyPosition);
    closePlacePage();
    mSearchController.hide();
  }

  private void hidePositionChooser()
  {
    UiUtils.hide(mPositionChooser);
    Framework.nativeTurnOffChoosePositionMode();
    setFullscreen(false);
  }

  private void initMap()
  {
    mFadeView = findViewById(R.id.fade_view);
    mFadeView.setListener(new FadeView.Listener()
    {
      @Override
      public boolean onTouch()
      {
        return getCurrentMenu().close(true);
      }
    });

    mMapFragment = (MapFragment) getSupportFragmentManager().findFragmentByTag(MapFragment.class.getName());
    if (mMapFragment == null)
    {
      Bundle args = new Bundle();
      args.putBoolean(MapFragment.ARG_LAUNCH_BY_DEEP_LINK, mIsLaunchByDeepLink);
      mMapFragment = (MapFragment) MapFragment.instantiate(this, MapFragment.class.getName(), args);
      getSupportFragmentManager()
          .beginTransaction()
          .replace(R.id.map_fragment_container, mMapFragment, MapFragment.class.getName())
          .commit();
    }

    View container = findViewById(R.id.map_fragment_container);
    if (container != null)
    {
      container.setOnTouchListener(this);
    }
  }

  public boolean isMapAttached()
  {
    return mMapFragment != null && mMapFragment.isAdded();
  }

  private void initNavigationButtons()
  {
    View frame = findViewById(R.id.navigation_buttons);
    if (frame == null)
      return;

    View zoomIn = frame.findViewById(R.id.nav_zoom_in);
    zoomIn.setOnClickListener(this);
    View zoomOut = frame.findViewById(R.id.nav_zoom_out);
    zoomOut.setOnClickListener(this);
    View myPosition = frame.findViewById(R.id.my_position);
    mNavMyPosition = new MyPositionButton(myPosition, mOnMyPositionClickListener);

    initToggleMapLayerController(frame);
    mNavAnimationController = new NavigationButtonsAnimationController(
        zoomIn, zoomOut, myPosition, getWindow().getDecorView().getRootView(), this);
  }

  private void initToggleMapLayerController(@NonNull View frame)
  {
    ImageButton trafficBtn = frame.findViewById(R.id.traffic);
    TrafficButton traffic = new TrafficButton(trafficBtn);
    View subway = frame.findViewById(R.id.subway);
    mToggleMapLayerController = new MapLayerCompositeController(traffic, subway, this);
    mToggleMapLayerController.attachCore();
  }

  public boolean closePlacePage()
  {
    if (mPlacePageController.isClosed())
      return false;

    mPlacePageController.close();
    return true;
  }

  public boolean closeSidePanel()
  {
    if (interceptBackPress())
      return true;

    if (removeCurrentFragment(true))
    {
      InputUtils.hideKeyboard(mFadeView);
      mFadeView.fadeOut();
      return true;
    }

    return false;
  }

  private void closeAllFloatingPanels()
  {
    if (!mIsTabletLayout)
      return;

    closePlacePage();
    if (removeCurrentFragment(true))
    {
      InputUtils.hideKeyboard(mFadeView);
      mFadeView.fadeOut();
    }
  }

  public void closeMenu(@Nullable Runnable procAfterClose)
  {
    mFadeView.fadeOut();
    mMainMenu.close(true, procAfterClose);
  }
  private boolean closePositionChooser()
  {
    if (UiUtils.isVisible(mPositionChooser))
    {
      hidePositionChooser();
      return true;
    }

    return false;
  }

  public void startLocationToPoint(final @Nullable MapObject endPoint,
                                   final boolean canUseMyPositionAsStart)
  {
    closeMenu(() -> {
      RoutingController.get().prepare(canUseMyPositionAsStart, endPoint);

      // TODO: check for tablet.
      closePlacePage();
    });
  }

  private void toggleMenu()
  {
    getCurrentMenu().toggle(true);
    refreshFade();
  }

  public void refreshFade()
  {
    if (getCurrentMenu().isOpen())
      mFadeView.fadeIn();
    else
      mFadeView.fadeOut();
  }

  private void initMainMenu()
  {
    mMainMenu = new MainMenu(findViewById(R.id.menu_frame), this::onItemClickOrSkipAnim);

    if (mIsTabletLayout)
    {
      mPanelAnimator = new PanelAnimator(this);
      mPanelAnimator.registerListener(mMainMenu.getLeftAnimationTrackListener());
      return;
    }
  }

  private void onItemClickOrSkipAnim(@NonNull MainMenu.Item item)
  {
    if (mIsFullscreenAnimating)
      return;

    item.onClicked(this, item);
  }

  public void showDiscovery()
  {
    if (mIsTabletLayout)
    {
      replaceFragment(DiscoveryFragment.class, null, null);
    }
    else
    {
      Intent i = new Intent(MwmActivity.this, DiscoveryActivity.class);
      startActivityForResult(i, REQ_CODE_DISCOVERY);
    }
  }

  private void initOnmapDownloader()
  {
    mOnmapDownloader = new OnmapDownloader(this);
    if (mIsTabletLayout)
      mPanelAnimator.registerListener(mOnmapDownloader);
  }

  @Override
  protected void onSaveInstanceState(Bundle outState)
  {
    mPlacePageController.onSave(outState);
    if (!mIsTabletLayout && RoutingController.get().isPlanning())
      mRoutingPlanInplaceController.onSaveState(outState);

    if (mIsTabletLayout)
    {
      RoutingPlanFragment fragment = (RoutingPlanFragment) getFragment(RoutingPlanFragment.class);
      if (fragment != null)
        fragment.saveRoutingPanelState(outState);
    }

    mNavigationController.onSaveState(outState);

    RoutingController.get().onSaveState();
    outState.putBoolean(EXTRA_LOCATION_DIALOG_IS_ANNOYING, mLocationErrorDialogAnnoying);

    if (mFilterController != null)
      mFilterController.onSaveState(outState);

    if (!isChangingConfigurations())
      RoutingController.get().saveRoute();
    else
      // We no longer need in a saved route if it's a configuration changing: theme switching,
      // orientation changing, etc. Otherwise, the saved route might be restored at undesirable moment.
      RoutingController.get().deleteSavedRoute();

    super.onSaveInstanceState(outState);
  }

  @Override
  protected void onRestoreInstanceState(@NonNull Bundle savedInstanceState)
  {
    super.onRestoreInstanceState(savedInstanceState);
    mPlacePageController.onRestore(savedInstanceState);
    if (mIsTabletLayout)
    {
      RoutingPlanFragment fragment = (RoutingPlanFragment) getFragment(RoutingPlanFragment.class);
      if (fragment != null)
      {
        fragment.restoreRoutingPanelState(savedInstanceState);
      }
      else if (RoutingController.get().isPlanning())
      {
        mRestoreRoutingPlanFragmentNeeded = true;
        mSavedForTabletState = savedInstanceState;
      }
    }

    if (!mIsTabletLayout && RoutingController.get().isPlanning())
      mRoutingPlanInplaceController.restoreState(savedInstanceState);

    mNavigationController.onRestoreState(savedInstanceState);

    if (mFilterController != null)
      mFilterController.onRestoreState(savedInstanceState);
  }

  @Override
  protected void onActivityResult(int requestCode, int resultCode, Intent data)
  {
    super.onActivityResult(requestCode, resultCode, data);

    if (resultCode != Activity.RESULT_OK)
      return;

    switch (requestCode)
    {
      case REQ_CODE_DISCOVERY:
        handleDiscoveryResult(data);
        break;
      case FilterActivity.REQ_CODE_FILTER:
      case REQ_CODE_SHOW_SIMILAR_HOTELS:
        if (mIsTabletLayout)
        {
          showTabletSearch(data, getString(R.string.hotel));
          return;
        }
        handleFilterResult(data);
        break;
      case BookmarkCategoriesActivity.REQ_CODE_DOWNLOAD_BOOKMARK_CATEGORY:
        handleDownloadedCategoryResult(data);
        break;
      case REQ_CODE_DRIVING_OPTIONS:
        rebuildLastRoute();
        break;
    }
  }

  private void rebuildLastRoute()
  {
    RoutingController.get().attach(this);
    rebuildLastRouteInternal();
  }

  private void rebuildLastRouteInternal()
  {
    if (mRoutingPlanInplaceController == null)
      return;

    mRoutingPlanInplaceController.hideDrivingOptionsView();
    RoutingController.get().rebuildLastRoute();
  }

  @Override
  public void toggleRouteSettings(@NonNull RoadType roadType)
  {
    mPlacePageController.close();
    RoutingOptions.addOption(roadType);
    rebuildLastRouteInternal();
  }

  private void handleDownloadedCategoryResult(@NonNull Intent data)
  {
    BookmarkCategory category = data.getParcelableExtra(BookmarksCatalogActivity.EXTRA_DOWNLOADED_CATEGORY);
    if (category == null)
      throw new IllegalArgumentException("Category not found in bundle");

    MapTask mapTask = target -> showBookmarkCategory(category);
    addTask(mapTask);
  }

  private boolean showBookmarkCategory(@NonNull BookmarkCategory category)
  {
    Framework.nativeShowBookmarkCategory(category.getId());
    return true;
  }

  private void handleDiscoveryResult(@NonNull Intent data)
  {
    String action = data.getAction();
    if (TextUtils.isEmpty(action))
      return;

    switch (action)
    {
      case DiscoveryActivity.ACTION_ROUTE_TO:
        MapObject destination = data.getParcelableExtra(DiscoveryActivity.EXTRA_DISCOVERY_OBJECT);
        if (destination == null)
          return;

        onRouteToDiscoveredObject(destination);
        break;

      case DiscoveryActivity.ACTION_SHOW_ON_MAP:
        destination = data.getParcelableExtra(DiscoveryActivity.EXTRA_DISCOVERY_OBJECT);
        if (destination == null)
          return;

        onShowDiscoveredObject(destination);
        break;

      case DiscoveryActivity.ACTION_SHOW_FILTER_RESULTS:
        handleFilterResult(data);
        break;
    }
  }

  private void handleFilterResult(@Nullable Intent data)
  {
    if (data == null || mFilterController == null)
      return;

    setupSearchQuery(data);

    BookingFilterParams params = data.getParcelableExtra(FilterActivity.EXTRA_FILTER_PARAMS);
    mFilterController.setFilterAndParams(data.getParcelableExtra(FilterActivity.EXTRA_FILTER),
                                         params);
    mFilterController.updateFilterButtonVisibility(params != null);
    runSearch();
  }

  private void setupSearchQuery(@NonNull Intent data)
  {
    if (mSearchController == null)
      return;

    String query = data.getStringExtra(DiscoveryActivity.EXTRA_FILTER_SEARCH_QUERY);
    mSearchController.setQuery(TextUtils.isEmpty(query) ? getString(R.string.hotel) + " " : query);
  }

  private void runHotelCategorySearchOnMap()
  {
    if (mSearchController == null || mFilterController == null)
      return;

    mSearchController.setQuery(getActivity().getString(R.string.hotel) + " ");
    runSearch();

    mSearchController.refreshToolbar();
    mFilterController.updateFilterButtonVisibility(true);
    mFilterController.show(true, true);
  }

  @Override
  public void onRouteToDiscoveredObject(@NonNull final MapObject object)
  {
    addTask((MapTask) target ->
    {
      RoutingController.get().setRouterType(Framework.ROUTER_TYPE_PEDESTRIAN);
      RoutingController.get().prepare(true, object);
      return false;
    });
  }

  @Override
  public void onShowDiscoveredObject(@NonNull final MapObject object)
  {
    addTask((MapTask) target ->
    {
      Framework.nativeShowFeatureByLatLon(object.getLat(), object.getLon());
      return false;
    });
  }

  @Override
  public void onShowFilter()
  {
    FilterActivity.startForResult(MwmActivity.this, null, null,
                                  FilterActivity.REQ_CODE_FILTER);
  }

  @Override
  public void onShowSimilarObjects(@NonNull Items.SearchItem item, @NonNull ItemType type)
  {
    String query = getString(type.getSearchCategory());
    showSearch(query);
  }

  public void onSearchSimilarHotels(@Nullable HotelsFilter filter)
  {
    BookingFilterParams params = mFilterController != null
                                 ? mFilterController.getBookingFilterParams() : null;
    FilterActivity.startForResult(MwmActivity.this, filter, params,
                                  REQ_CODE_SHOW_SIMILAR_HOTELS);
  }

  @Override
  public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
                                         @NonNull int[] grantResults)
  {
    super.onRequestPermissionsResult(requestCode, permissions, grantResults);
    if (requestCode != REQ_CODE_LOCATION_PERMISSION || grantResults.length == 0)
      return;

    PermissionsResult result = PermissionsUtils.computePermissionsResult(permissions, grantResults);
    if (result.isLocationGranted())
      myPositionClick();
  }

  @Override
  public void onSubwayLayerSelected()
  {
    mToggleMapLayerController.toggleMode(Mode.SUBWAY);
  }

  @Override
  public void onTrafficLayerSelected()
  {
    mToggleMapLayerController.toggleMode(Mode.TRAFFIC);
  }

  @Override
  public void onImportStarted(@NonNull String serverId)
  {
    // Do nothing by default.
  }

  @Override
  public void onImportFinished(@NonNull String serverId, long catId, boolean successful)
  {
    if (!successful)
      return;

    Toast.makeText(this, R.string.guide_downloaded_title, Toast.LENGTH_LONG).show();
    Statistics.INSTANCE.trackEvent(Statistics.EventName.BM_GUIDEDOWNLOADTOAST_SHOWN);
  }

  @Override
  public void onTagsReceived(boolean successful, @NonNull List<CatalogTagsGroup> tagsGroups,
                             int tagsLimit)
  {
    //TODO(@alexzatsepin): Implement me if necessary
  }

  @Override
  public void onCustomPropertiesReceived(boolean successful,
                                         @NonNull List<CatalogCustomProperty> properties)
  {
    //TODO(@alexzatsepin): Implement me if necessary
  }

  @Override
  public void onUploadStarted(long originCategoryId)
  {
    //TODO(@alexzatsepin): Implement me if necessary
  }

  @Override
  public void onUploadFinished(@NonNull BookmarkManager.UploadResult uploadResult, @NonNull String description,
                               long originCategoryId, long resultCategoryId)
  {
    //TODO(@alexzatsepin): Implement me if necessary
  }

  @Override
  protected void onNewIntent(Intent intent)
  {
    super.onNewIntent(intent);
    setIntent(intent);
    processIntent(intent);
  }

  private boolean processIntent(Intent intent)
  {
    if (intent == null)
      return false;

    final Notifier notifier = Notifier.from(getApplication());
    notifier.processNotificationExtras(intent);

    if (intent.hasExtra(EXTRA_TASK))
    {
      addTask(intent);
      return true;
    }

    HotelsFilter filter = intent.getParcelableExtra(FilterActivity.EXTRA_FILTER);
    BookingFilterParams params = intent.getParcelableExtra(FilterActivity.EXTRA_FILTER_PARAMS);
    if (mFilterController != null && (filter != null || params != null))
    {
      mFilterController.updateFilterButtonVisibility(true);
      mFilterController.show(!TextUtils.isEmpty(SearchEngine.INSTANCE.getQuery()), true);
      mFilterController.setFilterAndParams(filter, params);
      return true;
    }

    return false;
  }

  private void addTask(Intent intent)
  {
    if (intent != null &&
        !intent.getBooleanExtra(EXTRA_CONSUMED, false) &&
        intent.hasExtra(EXTRA_TASK) &&
        ((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY) == 0))
    {
      final MapTask mapTask = (MapTask) intent.getSerializableExtra(EXTRA_TASK);
      mTasks.add(mapTask);
      intent.removeExtra(EXTRA_TASK);

      if (isMapRendererActive())
        runTasks();

      // mark intent as consumed
      intent.putExtra(EXTRA_CONSUMED, true);
    }
  }

  private boolean isMapRendererActive()
  {
    return mMapFragment != null && MapFragment.nativeIsEngineCreated()
           && mMapFragment.isContextCreated();
  }

  private void addTask(MapTask task)
  {
    mTasks.add(task);
    if (isMapRendererActive())
      runTasks();
  }

  @CallSuper
  @Override
  protected void onResume()
  {
    super.onResume();
    mSearchController.refreshToolbar();
    mMainMenu.onResume(new Runnable()
    {
      @Override
      public void run()
      {
        if (Framework.nativeIsInChoosePositionMode())
        {
          UiUtils.show(mPositionChooser);
          setFullscreen(true);
        }
      }
    });
    if (mOnmapDownloader != null)
      mOnmapDownloader.onResume();

    mNavigationController.onResume();

    if (mNavAnimationController != null)
      mNavAnimationController.onResume();
    mPlacePageController.onActivityResumed(this);
  }

  @Override
  public void recreate()
  {
    // Explicitly destroy surface before activity recreation.
    if (mMapFragment != null)
      mMapFragment.destroySurface();
    super.recreate();
  }

  @Override
  protected void onResumeFragments()
  {
    super.onResumeFragments();
    RoutingController.get().restore();

    if (!LikesManager.INSTANCE.isNewUser() && Counters.isShowReviewForOldUser())
    {
      LikesManager.INSTANCE.showRateDialogForOldUser(this);
      Counters.setShowReviewForOldUser(false);
    }
    else
    {
      LikesManager.INSTANCE.showDialogs(this);
    }
  }

  @Override
  protected void onPause()
  {
    TtsPlayer.INSTANCE.stop();
    LikesManager.INSTANCE.cancelDialogs();
    if (mOnmapDownloader != null)
      mOnmapDownloader.onPause();
    mPlacePageController.onActivityPaused(this);
    super.onPause();
  }

  @Override
  protected void onStart()
  {
    super.onStart();
    SearchEngine.INSTANCE.addListener(this);
    Framework.nativeSetMapObjectListener(this);
    BookmarkManager.INSTANCE.addLoadingListener(this);
    BookmarkManager.INSTANCE.addCatalogListener(this);
    RoutingController.get().attach(this);
    if (MapFragment.nativeIsEngineCreated())
      LocationHelper.INSTANCE.attach(this);
    mPlacePageController.onActivityStarted(this);
  }

  @Override
  protected void onStop()
  {
    super.onStop();
    SearchEngine.INSTANCE.removeListener(this);
    Framework.nativeRemoveMapObjectListener();
    BookmarkManager.INSTANCE.removeLoadingListener(this);
    BookmarkManager.INSTANCE.removeCatalogListener(this);
    LocationHelper.INSTANCE.detach(!isFinishing());
    RoutingController.get().detach();
    mPlacePageController.onActivityStopped(this);
  }

  @CallSuper
  @Override
  protected void onSafeDestroy()
  {
    super.onSafeDestroy();
    if (mAdsRemovalPurchaseController != null)
      mAdsRemovalPurchaseController.destroy();
    if (mBookmarkPurchaseController != null)
      mBookmarkPurchaseController.destroy();

    mNavigationController.destroy();
    mToggleMapLayerController.detachCore();
    TrafficManager.INSTANCE.detachAll();
    mPlacePageController.destroy();
  }

  @Override
  public void onBackPressed()
  {
    if (getCurrentMenu().close(true))
    {
      mFadeView.fadeOut();
      return;
    }

    if (mSearchController != null && mSearchController.hide())
    {
      SearchEngine.INSTANCE.cancelInteractiveSearch();
      if (mFilterController != null)
        mFilterController.resetFilter();
      mSearchController.clear();
      return;
    }

    boolean isRoutingCancelled = RoutingController.get().cancel();
    if (isRoutingCancelled)
    {
      @Framework.RouterType
      int type = RoutingController.get().getLastRouterType();
      Statistics.INSTANCE.trackRoutingFinish(true, type,
                                             TrafficManager.INSTANCE.isEnabled());
    }

    if (!closePlacePage() && !closeSidePanel() && !isRoutingCancelled
        && !closePositionChooser())
    {
      try
      {
        super.onBackPressed();
      } catch (IllegalStateException e)
      {
        // Sometimes this can be called after onSaveState() for unknown reason.
      }
    }
  }

  private boolean interceptBackPress()
  {
    final FragmentManager manager = getSupportFragmentManager();
    for (String tag : DOCKED_FRAGMENTS)
    {
      final Fragment fragment = manager.findFragmentByTag(tag);
      if (fragment != null && fragment.isResumed() && fragment instanceof OnBackPressListener)
        return ((OnBackPressListener) fragment).onBackPressed();
    }

    return false;
  }

  private void removeFragmentImmediate(Fragment fragment)
  {
    FragmentManager fm = getSupportFragmentManager();
    if (fm.isDestroyed())
      return;

    fm.beginTransaction()
      .remove(fragment)
      .commitAllowingStateLoss();
    fm.executePendingTransactions();
  }

  private boolean removeCurrentFragment(boolean animate)
  {
    for (String tag : DOCKED_FRAGMENTS)
      if (removeFragment(tag, animate))
        return true;

    return false;
  }

  private boolean removeFragment(String className, boolean animate)
  {
    if (animate && mPanelAnimator == null)
      animate = false;

    final Fragment fragment = getSupportFragmentManager().findFragmentByTag(className);
    if (fragment == null)
      return false;

    if (animate)
      mPanelAnimator.hide(new Runnable()
      {
        @Override
        public void run()
        {
          removeFragmentImmediate(fragment);
        }
      });
    else
      removeFragmentImmediate(fragment);

    return true;
  }

  // Called from JNI.
  @Override
  public void onMapObjectActivated(final MapObject object)
  {
    if (MapObject.isOfType(MapObject.API_POINT, object))
    {
      final ParsedMwmRequest request = ParsedMwmRequest.getCurrentRequest();
      if (request == null)
        return;

      request.setPointData(object.getLat(), object.getLon(), object.getTitle(), object.getApiId());
      object.setSubtitle(request.getCallerName(MwmApplication.get()).toString());
    }

    setFullscreen(false);

    mPlacePageController.openFor(object);

    if (UiUtils.isVisible(mFadeView))
      mFadeView.fadeOut();
  }

  // Called from JNI.
  @Override
  public void onDismiss(boolean switchFullScreenMode)
  {
    if (switchFullScreenMode)
    {
      if ((mPanelAnimator != null && mPanelAnimator.isVisible()) ||
           UiUtils.isVisible(mSearchController.getToolbar()))
        return;

      setFullscreen(!mIsFullscreen);
    }
    else
    {
      mPlacePageController.close();
    }
  }

  private BaseMenu getCurrentMenu()
  {
    return (RoutingController.get().isNavigating()
            ? mNavigationController.getNavMenu()
            : mMainMenu);
  }

  private void setFullscreen(boolean isFullscreen)
  {
    if (RoutingController.get().isNavigating()
            || RoutingController.get().isBuilding()
            || RoutingController.get().isPlanning())
      return;

    mIsFullscreen = isFullscreen;
    final BaseMenu menu = getCurrentMenu();

    if (isFullscreen)
    {
      if (menu.isAnimating())
        return;

      mIsFullscreenAnimating = true;
      UiUtils.invisible(menu.getFrame());

      final int menuHeight = menu.getFrame().getHeight();
      adjustBottomWidgets(menuHeight);

      mIsFullscreenAnimating = false;
      if (mIsAppearMenuLater)
      {
        appearMenu(menu);
        mIsAppearMenuLater = false;
      }

      if (mNavAnimationController != null)
        mNavAnimationController.disappearZoomButtons();
      if (mNavMyPosition != null)
        mNavMyPosition.hide();
      mToggleMapLayerController.hide();
    }
    else
    {
      if (mPlacePageController.isClosed() && mNavAnimationController != null)
        mNavAnimationController.appearZoomButtons();
      if (!mIsFullscreenAnimating)
        appearMenu(menu);
      else
        mIsAppearMenuLater = true;
    }
  }

  private void appearMenu(BaseMenu menu)
  {
    appearMenuFrame(menu);
    showNavMyPositionBtn();
    mToggleMapLayerController.applyLastActiveMode();
  }

  private void showNavMyPositionBtn()
  {
    if (mNavMyPosition != null)
      mNavMyPosition.show();
  }

  private void appearMenuFrame(@NonNull BaseMenu menu)
  {
    UiUtils.show(menu.getFrame());
    adjustBottomWidgets(0);
  }

  @Override
  public void onPlacePageSlide(int top)
  {
    if (mNavAnimationController != null)
      mNavAnimationController.move(top);
  }

  @Override
  public void onClick(View v)
  {
    switch (v.getId())
    {
    case R.id.nav_zoom_in:
      Statistics.INSTANCE.trackEvent(Statistics.EventName.ZOOM_IN);
      AlohaHelper.logClick(AlohaHelper.ZOOM_IN);
      MapFragment.nativeScalePlus();
      break;
    case R.id.nav_zoom_out:
      Statistics.INSTANCE.trackEvent(Statistics.EventName.ZOOM_OUT);
      AlohaHelper.logClick(AlohaHelper.ZOOM_OUT);
      MapFragment.nativeScaleMinus();
      break;
    }
  }

  @Override
  public boolean onTouch(View view, MotionEvent event)
  {
    return mMapFragment != null && mMapFragment.onTouch(view, event);
  }

  @Override
  public void customOnNavigateUp()
  {
    if (removeCurrentFragment(true))
    {
      InputUtils.hideKeyboard(mMainMenu.getFrame());
      mSearchController.refreshToolbar();
    }
  }



  void adjustCompass(int offsetY)
  {
    if (mMapFragment == null || !mMapFragment.isAdded())
      return;

    int resultOffset = offsetY;
    //If the compass is covered by navigation buttons, we move it beyond the visible screen
    if (mNavAnimationController != null && mNavAnimationController.isConflictWithCompass(offsetY))
    {
      int halfHeight = (int)(UiUtils.dimen(R.dimen.compass_height) * 0.5f);
      int margin = UiUtils.dimen(R.dimen.margin_compass_top)
                   + UiUtils.dimen(R.dimen.nav_frame_padding);
      resultOffset = -(offsetY + halfHeight + margin);
    }

    mMapFragment.setupCompass(resultOffset, true);

    CompassData compass = LocationHelper.INSTANCE.getCompassData();
    if (compass != null)
      MapFragment.nativeCompassUpdated(compass.getMagneticNorth(), compass.getTrueNorth(), true);
  }

  private void adjustBottomWidgets(int offsetY)
  {
    if (mMapFragment == null || !mMapFragment.isAdded())
      return;

    mMapFragment.setupRuler(offsetY, false);
    mMapFragment.setupWatermark(offsetY, true);
  }

  @Override
  public FragmentActivity getActivity()
  {
    return this;
  }

  public MainMenu getMainMenu()
  {
    return mMainMenu;
  }

  @Override
  public void showSearch()
  {
    showSearch("");
  }

  @Override
  public void updateMenu()
  {
    adjustMenuLineFrameVisibility();
    mNavigationController.showSearchButtons(RoutingController.get().isPlanning()
                                            || RoutingController.get().isBuilt());

    if (RoutingController.get().isNavigating())
    {
      mNavigationController.show(true);
      mSearchController.hide();
      mMainMenu.setState(MainMenu.State.NAVIGATION, false, mIsFullscreen);
      return;
    }

    if (mIsTabletLayout)
    {
      mMainMenu.setEnabled(MainMenu.Item.POINT_TO_POINT, !RoutingController.get().isPlanning());
      mMainMenu.setEnabled(MainMenu.Item.SEARCH, !RoutingController.get().isWaitingPoiPick());
    }
    else if (RoutingController.get().isPlanning())
    {
      mMainMenu.setState(MainMenu.State.ROUTE_PREPARE, false, mIsFullscreen);
      return;
    }

    mMainMenu.setState(MainMenu.State.MENU, false, mIsFullscreen);
  }

  @Override
  @Nullable
  public PurchaseController<PurchaseCallback> getAdsRemovalPurchaseController()
  {
    return mAdsRemovalPurchaseController;
  }

  @Override
  public void onAdsRemovalActivation()
  {
    closePlacePage();
  }

  private void adjustMenuLineFrameVisibility()
  {
    final RoutingController controller = RoutingController.get();

    if (controller.isBuilt() || controller.isTaxiRequestHandled())
    {
      showLineFrame();
      return;
    }

    if (controller.isPlanning() || controller.isBuilding() || controller.isErrorEncountered())
    {
      if (showAddStartOrFinishFrame(controller, true))
      {
        return;
      }

      showLineFrame(false);
      final int menuHeight = getCurrentMenu().getFrame().getHeight();
      adjustBottomWidgets(menuHeight);
      return;
    }

    hideRoutingActionFrame();
    showLineFrame();
  }

  private boolean showAddStartOrFinishFrame(@NonNull RoutingController controller,
                                            boolean showFrame)
  {
    // S - start, F - finish, L - my position
    // -S-F-L -> Start
    // -S-F+L -> Finish
    // -S+F-L -> Start
    // -S+F+L -> Start + Use
    // +S-F-L -> Finish
    // +S-F+L -> Finish
    // +S+F-L -> Hide
    // +S+F+L -> Hide

    MapObject myPosition = LocationHelper.INSTANCE.getMyPosition();

    if (myPosition != null && !controller.hasEndPoint())
    {
      showAddFinishFrame();
      if (showFrame)
        showLineFrame();
      return true;
    }
    if (!controller.hasStartPoint())
    {
      showAddStartFrame();
      if (showFrame)
        showLineFrame();
      return true;
    }
    if (!controller.hasEndPoint())
    {
      showAddFinishFrame();
      if (showFrame)
        showLineFrame();
      return true;
    }

    return false;
  }

  private void showAddStartFrame()
  {
    if (!mIsTabletLayout)
    {
      mRoutingPlanInplaceController.showAddStartFrame();
      return;
    }

    RoutingPlanFragment fragment = (RoutingPlanFragment) getFragment(RoutingPlanFragment.class);
    if (fragment != null)
      fragment.showAddStartFrame();
  }

  private void showAddFinishFrame()
  {
    if (!mIsTabletLayout)
    {
      mRoutingPlanInplaceController.showAddFinishFrame();
      return;
    }

    RoutingPlanFragment fragment = (RoutingPlanFragment) getFragment(RoutingPlanFragment.class);
    if (fragment != null)
      fragment.showAddFinishFrame();
  }

  private void hideRoutingActionFrame()
  {
    if (!mIsTabletLayout)
    {
      mRoutingPlanInplaceController.hideActionFrame();
      return;
    }

    RoutingPlanFragment fragment = (RoutingPlanFragment) getFragment(RoutingPlanFragment.class);
    if (fragment != null)
      fragment.hideActionFrame();
  }

  private void showLineFrame()
  {
    showLineFrame(true);
    adjustBottomWidgets(0);
  }

  private void showLineFrame(boolean show)
  {
    mMainMenu.showLineFrame(show);
  }

  private void setNavButtonsTopLimit(int limit)
  {
    if (mNavAnimationController == null)
      return;

    mNavAnimationController.setTopLimit(limit);
  }

  @Override
  public void onRoutingPlanStartAnimate(boolean show)
  {
    if (mNavAnimationController == null)
      return;

    int totalHeight = calcFloatingViewsOffset();

    mNavAnimationController.setTopLimit(!show ? 0 : totalHeight);
    mNavAnimationController.setBottomLimit(!show ? 0 : getCurrentMenu().getFrame().getHeight());
    adjustCompassAndTraffic(!show ? UiUtils.getStatusBarHeight(getApplicationContext())
                                  : totalHeight);
  }

  @Override
  public void showRoutePlan(boolean show, @Nullable Runnable completionListener)
  {
    if (show)
    {
      mSearchController.hide();

      if (mIsTabletLayout)
      {
        replaceFragment(RoutingPlanFragment.class, null, completionListener);
        if (mRestoreRoutingPlanFragmentNeeded && mSavedForTabletState != null)
        {
          RoutingPlanFragment fragment = (RoutingPlanFragment) getFragment(RoutingPlanFragment.class);
          if (fragment != null)
            fragment.restoreRoutingPanelState(mSavedForTabletState);
        }
        showAddStartOrFinishFrame(RoutingController.get(), false);
        int width = UiUtils.dimen(R.dimen.panel_width);
        adjustTraffic(width, UiUtils.getStatusBarHeight(getApplicationContext()));
        mNavigationController.adjustSearchButtons(width);
      }
      else
      {
        mRoutingPlanInplaceController.show(true);
        if (completionListener != null)
          completionListener.run();
      }
    }
    else
    {
      if (mIsTabletLayout)
      {
        adjustCompassAndTraffic(UiUtils.getStatusBarHeight(getApplicationContext()));
        setNavButtonsTopLimit(0);
        mNavigationController.adjustSearchButtons(0);
      }
      else
      {
        mRoutingPlanInplaceController.show(false);
      }

      closeAllFloatingPanels();
      mNavigationController.resetSearchWheel();

      if (completionListener != null)
        completionListener.run();

      updateSearchBar();
    }

    // TODO:
//    mPlacePage.refreshViews();
  }

  private void adjustCompassAndTraffic(final int offsetY)
  {
    addTask(new MapTask()
    {
      @Override
      public boolean run(@NonNull MwmActivity target)
      {
        adjustCompass(offsetY);
        return true;
      }
    });
    adjustTraffic(0, offsetY);
  }

  private void adjustTraffic(int offsetX, int offsetY)
  {
    mToggleMapLayerController.adjust(offsetX, offsetY);
  }

  @Override
  public void onSearchVisibilityChanged(boolean visible)
  {
    if (mNavAnimationController == null)
      return;

    int toolbarHeight = mSearchController.getToolbar().getHeight();
    int offset = calcFloatingViewsOffset();

    adjustCompassAndTraffic(visible ? toolbarHeight : offset);
    setNavButtonsTopLimit(visible ? toolbarHeight : 0);
    if (mFilterController != null)
    {
      boolean show = visible && !TextUtils.isEmpty(SearchEngine.INSTANCE.getQuery())
                     && !RoutingController.get().isNavigating();
      mFilterController.show(show, true);
      mMainMenu.show(!show);
    }
  }

  private int calcFloatingViewsOffset()
  {
    int offset;
    if (mRoutingPlanInplaceController == null
        || (offset = mRoutingPlanInplaceController.calcHeight()) == 0)
      return UiUtils.getStatusBarHeight(this);

    return offset;
  }

  @Override
  public void onResultsUpdate(SearchResult[] results, long timestamp, boolean isHotel)
  {
    if (mFilterController != null)
      mFilterController.updateFilterButtonVisibility(isHotel);
  }

  @Override
  public void onResultsEnd(long timestamp)
  {
  }

  @Override
  public void showNavigation(boolean show)
  {
    // TODO:
//    mPlacePage.refreshViews();
    mNavigationController.show(show);
    refreshFade();
    if (mOnmapDownloader != null)
      mOnmapDownloader.updateState(false);
    if (show)
    {
      mSearchController.clear();
      mSearchController.hide();
      if (mFilterController != null)
        mFilterController.show(false, true);
    }
  }

  @Override
  public void updateBuildProgress(int progress, @Framework.RouterType int router)
  {
    if (mIsTabletLayout)
    {
      RoutingPlanFragment fragment = (RoutingPlanFragment) getFragment(RoutingPlanFragment.class);
      if (fragment != null)
        fragment.updateBuildProgress(progress, router);
    }
    else
    {
      mRoutingPlanInplaceController.updateBuildProgress(progress, router);
    }
  }

  @Override
  public void onStartRouteBuilding()
  {
    if (mRoutingPlanInplaceController == null)
      return;

    mRoutingPlanInplaceController.hideDrivingOptionsView();
  }

  @Override
  public void onTaxiInfoReceived(@NonNull TaxiInfo info)
  {
    if (mIsTabletLayout)
    {
      RoutingPlanFragment fragment = (RoutingPlanFragment) getFragment(RoutingPlanFragment.class);
      if (fragment != null)
        fragment.showTaxiInfo(info);
    }
    else
    {
      mRoutingPlanInplaceController.showTaxiInfo(info);
    }
  }

  @Override
  public void onTaxiError(@NonNull TaxiManager.ErrorCode code)
  {
    if (mIsTabletLayout)
    {
      RoutingPlanFragment fragment = (RoutingPlanFragment) getFragment(RoutingPlanFragment.class);
      if (fragment != null)
        fragment.showTaxiError(code);
    }
    else
    {
      mRoutingPlanInplaceController.showTaxiError(code);
    }
  }

  @Override
  public void onNavigationCancelled()
  {
    mNavigationController.stop(this);
    updateSearchBar();
    ThemeSwitcher.restart(isMapRendererActive());
    if (mRoutingPlanInplaceController == null)
      return;

    mRoutingPlanInplaceController.hideDrivingOptionsView();
  }

  @Override
  public void onNavigationStarted()
  {
    ThemeSwitcher.restart(isMapRendererActive());
  }

  @Override
  public void onAddedStop()
  {
    closePlacePage();
  }

  @Override
  public void onRemovedStop()
  {
    closePlacePage();
  }

  @Override
  public void onBuiltRoute()
  {
    if (!RoutingController.get().isPlanning())
      return;

    mNavigationController.resetSearchWheel();
  }

  @Override
  public void onDrivingOptionsWarning()
  {
    if (mRoutingPlanInplaceController == null)
      return;

    mRoutingPlanInplaceController.showDrivingOptionView();
  }

  @Override
  public boolean isSubwayEnabled()
  {
    return SubwayManager.from(this).isEnabled();
  }

  @Override
  public void onCommonBuildError(int lastResultCode, @NonNull String[] lastMissingMaps)
  {
    RoutingErrorDialogFragment fragment = RoutingErrorDialogFragment.create(lastResultCode, lastMissingMaps);
    fragment.show(getSupportFragmentManager(), RoutingErrorDialogFragment.class.getSimpleName());
  }

  @Override
  public void onDrivingOptionsBuildError()
  {
    com.mapswithme.maps.dialog.AlertDialog dialog =
        new com.mapswithme.maps.dialog.AlertDialog.Builder()
            .setTitleId(R.string.unable_to_calc_alert_title)
            .setMessageId(R.string.unable_to_calc_alert_subtitle)
            .setPositiveBtnId(R.string.settings)
            .setNegativeBtnId(R.string.cancel)
            .setReqCode(REQ_CODE_ERROR_DRIVING_OPTIONS_DIALOG)
            .setDialogFactory(new DrivingOptionsDialogFactory())
            .setFragManagerStrategyType(com.mapswithme.maps.dialog.AlertDialog
                                            .FragManagerStrategyType.ACTIVITY_FRAGMENT_MANAGER)
            .build();
    dialog.show(this, ERROR_DRIVING_OPTIONS_DIALOG_TAG);
  }

  private void updateSearchBar()
  {
    if (!TextUtils.isEmpty(SearchEngine.INSTANCE.getQuery()))
      mSearchController.refreshToolbar();
  }

  @Override
  public void onMyPositionModeChanged(int newMode)
  {
    if (mNavMyPosition != null)
      mNavMyPosition.update(newMode);

    RoutingController controller = RoutingController.get();
    if (controller.isPlanning())
      showAddStartOrFinishFrame(controller, true);
  }

  @Override
  public void onLocationUpdated(@NonNull Location location)
  {
    if (!RoutingController.get().isNavigating())
      return;

    mNavigationController.update(Framework.nativeGetRouteFollowingInfo());

    TtsPlayer.INSTANCE.playTurnNotifications(getApplicationContext());
  }

  @Override
  public void onCompassUpdated(@NonNull CompassData compass)
  {
    MapFragment.nativeCompassUpdated(compass.getMagneticNorth(), compass.getTrueNorth(), false);
    mNavigationController.updateNorth(compass.getNorth());
  }

  @Override
  public void onLocationError()
  {
    if (mLocationErrorDialogAnnoying)
      return;

    Intent intent = TargetUtils.makeAppSettingsLocationIntent(getApplicationContext());
    if (intent == null)
      return;
    showLocationErrorDialog(intent);
  }

  @Override
  public void onTranslationChanged(float translation)
  {
    mNavigationController.updateSearchButtonsTranslation(translation);
  }

  @Override
  public void onFadeInZoomButtons()
  {
    if (RoutingController.get().isPlanning() || RoutingController.get().isNavigating())
      mNavigationController.fadeInSearchButtons();
  }

  @Override
  public void onFadeOutZoomButtons()
  {
    if (RoutingController.get().isPlanning() || RoutingController.get().isNavigating())
    {
      if (UiUtils.isLandscape(this))
        mToggleMapLayerController.hide();
      else
        mNavigationController.fadeOutSearchButtons();
    }
  }

  private void showLocationErrorDialog(@NonNull final Intent intent)
  {
    if (mLocationErrorDialog != null && mLocationErrorDialog.isShowing())
      return;

    mLocationErrorDialog = new AlertDialog.Builder(this)
        .setTitle(R.string.enable_location_services)
        .setMessage(R.string.location_is_disabled_long_text)
        .setNegativeButton(R.string.close, new DialogInterface.OnClickListener()
        {
          @Override
          public void onClick(DialogInterface dialog, int which)
          {
            mLocationErrorDialogAnnoying = true;
          }
        })
        .setOnCancelListener(new DialogInterface.OnCancelListener()
        {
          @Override
          public void onCancel(DialogInterface dialog)
          {
            mLocationErrorDialogAnnoying = true;
          }
        })
        .setPositiveButton(R.string.connection_settings, new DialogInterface.OnClickListener()
        {
          @Override
          public void onClick(DialogInterface dialog, int which)
          {
            startActivity(intent);
          }
        }).show();
  }

  @Override
  public void onLocationNotFound()
  {
    showLocationNotFoundDialog();
  }

  @Override
  public void onRoutingFinish()
  {
    Statistics.INSTANCE.trackRoutingFinish(false, RoutingController.get().getLastRouterType(),
                                           TrafficManager.INSTANCE.isEnabled());
  }

  private void showLocationNotFoundDialog()
  {
    String message = String.format("%s\n\n%s", getString(R.string.current_location_unknown_message),
                                   getString(R.string.current_location_unknown_title));

    DialogInterface.OnClickListener stopClickListener = (dialog, which) ->
    {
      LocationHelper.INSTANCE.setStopLocationUpdateByUser(true);
    };

    DialogInterface.OnClickListener continueClickListener = (dialog, which) ->
    {
      if (!LocationHelper.INSTANCE.isActive())
        LocationHelper.INSTANCE.start();
    };

    new AlertDialog.Builder(this)
        .setMessage(message)
        .setNegativeButton(R.string.current_location_unknown_stop_button, stopClickListener)
        .setPositiveButton(R.string.current_location_unknown_continue_button, continueClickListener)
        .show();
  }

  @Override
  public void onUseMyPositionAsStart()
  {
    RoutingController.get().setStartPoint(LocationHelper.INSTANCE.getMyPosition());
  }

  @Override
  public void onSearchRoutePoint(@RoutePointInfo.RouteMarkType int pointType)
  {
    RoutingController.get().waitForPoiPick(pointType);
    mNavigationController.performSearchClick();
    Statistics.INSTANCE.trackRoutingTooltipEvent(pointType, true);
  }

  @Override
  public void onRoutingStart()
  {
    @Framework.RouterType
    int routerType = RoutingController.get().getLastRouterType();
    Statistics.INSTANCE.trackRoutingStart(routerType, TrafficManager.INSTANCE.isEnabled());
    closeMenu(() -> RoutingController.get().start());
  }

  @Override
  public void onBookmarksLoadingStarted()
  {
    // Do nothing
  }

  @Override
  public void onBookmarksLoadingFinished()
  {
    // Do nothing
  }


  @Override
  public void onAlertDialogPositiveClick(int requestCode, int which)
  {
    if (requestCode == REQ_CODE_ERROR_DRIVING_OPTIONS_DIALOG)
      DrivingOptionsActivity.start(this);
  }

  @Override
  public void onAlertDialogNegativeClick(int requestCode, int which)
  {
    // Do nothing
  }

  @Override
  public void onAlertDialogCancel(int requestCode)
  {
    // Do nothing
  }

  @Override
  public void onBookmarksFileLoaded(boolean success)
  {
    Utils.toastShortcut(MwmActivity.this, success ? R.string.load_kmz_successful :
        R.string.load_kmz_failed);
  }

  @Override
  public void onSearchClearClick()
  {
    if (mFilterController != null)
      mFilterController.resetFilter();
  }

  @Override
  public void onSearchUpClick(@Nullable String query)
  {
    showSearch(query);
  }

  @Override
  public void onSearchQueryClick(@Nullable String query)
  {
    showSearch(query);
  }

  public void showIntroductionScreenForDeeplink(@NonNull String deepLink,
                                                @NonNull IntroductionScreenFactory factory)
  {
    IntroductionDialogFragment.show(getSupportFragmentManager(), deepLink, factory);
  }

  private class CurrentPositionClickListener implements OnClickListener
  {
    @Override
    public void onClick(View v)
    {
      Statistics.INSTANCE.trackEvent(Statistics.EventName.TOOLBAR_MY_POSITION);
      AlohaHelper.logClick(AlohaHelper.TOOLBAR_MY_POSITION);

      if (!PermissionsUtils.isLocationGranted())
      {
        if (PermissionsUtils.isLocationExplanationNeeded(MwmActivity.this))
          PermissionsUtils.requestLocationPermission(MwmActivity.this, REQ_CODE_LOCATION_PERMISSION);
        else
          Toast.makeText(MwmActivity.this, R.string.enable_location_services, Toast.LENGTH_SHORT)
               .show();
        return;
      }

      myPositionClick();
    }
  }

  static abstract class AbstractClickMenuDelegate implements ClickMenuDelegate
  {
    @NonNull
    private final MwmActivity mActivity;
    @NonNull
    private final MainMenu.Item mItem;

    AbstractClickMenuDelegate(@NonNull MwmActivity activity, @NonNull MainMenu.Item item)
    {
      mActivity = activity;
      mItem = item;
    }

    @NonNull
    public MwmActivity getActivity()
    {
      return mActivity;
    }

    @NonNull
    public MainMenu.Item getItem()
    {
      return mItem;
    }

    @Override
    public final void onMenuItemClick()
    {
      TipsApi api = TipsApi.requestCurrent(getActivity(), getActivity().getClass());
      LOGGER.d(TAG, "TipsApi = " + api);
      if (getItem() == api.getSiblingMenuItem())
      {
        api.createClickInterceptor().onInterceptClick(getActivity());
        Statistics.INSTANCE.trackTipsEvent(Statistics.EventName.TIPS_TRICKS_CLICK, api.ordinal());
      }
      else
        onMenuItemClickInternal();
    }

    public abstract void onMenuItemClickInternal();
  }

  public static class MenuClickDelegate extends AbstractClickMenuDelegate
  {
    public MenuClickDelegate(@NonNull MwmActivity activity, @NonNull MainMenu.Item item)
    {
      super(activity, item);
    }

    @Override
    public void onMenuItemClickInternal()
    {
      if (!getActivity().mMainMenu.isOpen())
      {
        Statistics.INSTANCE.trackToolbarClick(getItem());
        // TODO:
        if (/*getActivity().mPlacePage.isDocked() &&*/ getActivity().closePlacePage())
          return;

        if (getActivity().closeSidePanel())
          return;
      }
      getActivity().toggleMenu();
    }
  }

  public static class AddPlaceDelegate extends StatisticClickMenuDelegate
  {
    public AddPlaceDelegate(@NonNull MwmActivity activity, @NonNull MainMenu.Item item)
    {
      super(activity, item);
    }

    @Override
    void onPostStatisticMenuItemClick()
    {
      getActivity().closePlacePage();
      if (getActivity().mIsTabletLayout)
        getActivity().closeSidePanel();
      getActivity().closeMenu(() -> getActivity().showPositionChooser(false, false));
    }
  }

  public static class SearchClickDelegate extends AbstractClickMenuDelegate
  {
    public SearchClickDelegate(@NonNull MwmActivity activity, @NonNull MainMenu.Item item)
    {
      super(activity, item);
    }

    @Override
    public void onMenuItemClickInternal()
    {
      Statistics.INSTANCE.trackToolbarClick(getItem());
      RoutingController.get().cancel();
      getActivity().closeMenu(() -> getActivity().showSearch(getActivity().mSearchController.getQuery()));
    }
  }

  public static class SettingsDelegate extends StatisticClickMenuDelegate
  {
    public SettingsDelegate(@NonNull MwmActivity activity, @NonNull MainMenu.Item item)
    {
      super(activity, item);
    }

    @Override
    void onPostStatisticMenuItemClick()
    {
      Intent intent = new Intent(getActivity(), SettingsActivity.class);
      getActivity().closeMenu(() -> getActivity().startActivity(intent));
    }
  }

  public static class DownloadGuidesDelegate extends StatisticClickMenuDelegate
  {
    public DownloadGuidesDelegate(@NonNull MwmActivity activity, @NonNull MainMenu.Item item)
    {
      super(activity, item);
    }

    @Override
    void onPostStatisticMenuItemClick()
    {
      int requestCode = BookmarkCategoriesActivity.REQ_CODE_DOWNLOAD_BOOKMARK_CATEGORY;
      String catalogUrl = BookmarkManager.INSTANCE.getCatalogFrontendUrl();
      getActivity().closeMenu(() -> BookmarksCatalogActivity.startForResult(getActivity(),
                                                                            requestCode,
                                                                            catalogUrl));
    }
  }

  public static class HotelSearchDelegate extends StatisticClickMenuDelegate
  {
    public HotelSearchDelegate(@NonNull MwmActivity activity, @NonNull MainMenu.Item item)
    {
      super(activity, item);
    }

    @Override
    void onPostStatisticMenuItemClick()
    {
      getActivity().closeMenu(() -> {
        getActivity().runHotelCategorySearchOnMap();
      });
    }
  }

  public abstract static class StatisticClickMenuDelegate extends AbstractClickMenuDelegate
  {
    StatisticClickMenuDelegate(@NonNull MwmActivity activity, @NonNull MainMenu.Item item)
    {
      super(activity, item);
    }

    @Override
    public void onMenuItemClickInternal()
    {
      Statistics.INSTANCE.trackToolbarMenu(getItem());
      onPostStatisticMenuItemClick();
    }

    abstract void onPostStatisticMenuItemClick();
  }

  public static class DownloadMapsDelegate extends StatisticClickMenuDelegate
  {
    public DownloadMapsDelegate(@NonNull MwmActivity activity, @NonNull MainMenu.Item item)
    {
      super(activity, item);
    }

    @Override
    void onPostStatisticMenuItemClick()
    {
      RoutingController.get().cancel();
      getActivity().closeMenu(() -> getActivity().showDownloader(false));
    }
  }

  public static class BookmarksDelegate extends StatisticClickMenuDelegate
  {
    public BookmarksDelegate(@NonNull MwmActivity activity, @NonNull MainMenu.Item item)
    {
      super(activity, item);
    }

    @Override
    void onPostStatisticMenuItemClick()
    {
      getActivity().closeMenu(getActivity()::showBookmarks);
    }
  }

  public static class ShareMyLocationDelegate extends StatisticClickMenuDelegate
  {
    public ShareMyLocationDelegate(@NonNull MwmActivity activity, @NonNull MainMenu.Item item)
    {
      super(activity, item);
    }

    @Override
    void onPostStatisticMenuItemClick()
    {
      getActivity().closeMenu(getActivity()::shareMyLocation);
    }
  }

  public static class DiscoveryDelegate extends StatisticClickMenuDelegate
  {
    public DiscoveryDelegate(@NonNull MwmActivity activity, @NonNull MainMenu.Item item)
    {
      super(activity, item);
    }

    @Override
    void onPostStatisticMenuItemClick()
    {
      getActivity().showDiscovery();
    }
  }

  public static class PointToPointDelegate extends StatisticClickMenuDelegate
  {
    public PointToPointDelegate(@NonNull MwmActivity activity, @NonNull MainMenu.Item item)
    {
      super(activity, item);
    }

    @Override
    void onPostStatisticMenuItemClick()
    {
      getActivity().startLocationToPoint(null, false);
    }
  }
}
