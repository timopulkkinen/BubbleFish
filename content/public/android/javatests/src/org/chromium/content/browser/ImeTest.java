// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.os.IBinder;
import android.os.ResultReceiver;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.TextUtils;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ImeAdapter.AdapterInputConnection;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_shell.ContentShellTestBase;

import java.util.concurrent.Callable;

public class ImeTest extends ContentShellTestBase {

    private static final int INVALID_SELECTION = -2;
    private static final int INVALID_COMPOSITION = -2;
    private static final String DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><meta name=\"viewport\"" +
            "content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" /></head>" +
            "<body><form action=\"about:blank\">" +
            "<input id=\"input_text\" type=\"text\" />" +
            "<input id=\"input_radio\" type=\"radio\" />" +
            "</form></body></html>");

    private TestAdapterInputConnection mConnection;
    private ImeAdapter mImeAdapter;
    private ContentView mContentView;
    private TestCallbackHelperContainer mCallbackContainer;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        launchContentShellWithUrl(DATA_URL);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());

        getImeAdapter().setInputMethodManagerWrapper(
                new TestInputMethodManagerWrapper(getContentViewCore()));
        getContentViewCore().setAdapterInputConnectionFactory(
                new TestAdapterInputConnectionFactory());

        mContentView = getActivity().getActiveContentView();
        mCallbackContainer = new TestCallbackHelperContainer(mContentView);
        // TODO(aurimas) remove this wait once crbug.com/179511 is fixed.
        assertWaitForPageScaleFactor(1);
        DOMUtils.clickNode(this, mContentView, mCallbackContainer, "input_text");
        assertWaitForKeyboardStatus(true);

        mConnection = (TestAdapterInputConnection) getAdapterInputConnection();
        mImeAdapter = getImeAdapter();

        assertWaitForSetEditableCallback(1, mConnection);
        assertEquals("", mConnection.mText);
        assertEquals(0, mConnection.mSelectionStart);
        assertEquals(0, mConnection.mSelectionEnd);
        assertEquals(-1, mConnection.mCompositionStart);
        assertEquals(-1, mConnection.mCompositionEnd);
    }

    @MediumTest
    @Feature({"TextInput", "Main"})
    public void testKeyboardDismissedAfterClickingGo() throws Throwable {
        mImeAdapter.checkCompositionQueueAndCallNative("hello", 1, false);
        assertWaitForSetEditableCallback(2, mConnection);
        assertEquals("hello", mConnection.mText);
        assertEquals(5, mConnection.mSelectionStart);
        assertEquals(5, mConnection.mSelectionEnd);
        assertEquals(0, mConnection.mCompositionStart);
        assertEquals(5, mConnection.mCompositionEnd);

        performGo(getAdapterInputConnection(), mCallbackContainer);

        assertWaitForSetEditableCallback(3, mConnection);
        assertEquals("", mConnection.mText);
        assertEquals(0, mConnection.mSelectionStart);
        assertEquals(0, mConnection.mSelectionEnd);
        assertEquals(-1, mConnection.mCompositionStart);
        assertEquals(-1, mConnection.mCompositionEnd);
        assertWaitForKeyboardStatus(false);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testGetTextUpdatesAfterEnteringText() throws Throwable {
        mImeAdapter.checkCompositionQueueAndCallNative("h", 1, false);
        assertWaitForSetEditableCallback(2, mConnection);
        assertEquals("h", mConnection.mText);
        assertEquals(1, mConnection.mSelectionStart);
        assertEquals(1, mConnection.mSelectionEnd);
        assertEquals(0, mConnection.mCompositionStart);
        assertEquals(1, mConnection.mCompositionEnd);

        mImeAdapter.checkCompositionQueueAndCallNative("he", 1, false);
        assertWaitForSetEditableCallback(3, mConnection);
        assertEquals("he", mConnection.mText);
        assertEquals(2, mConnection.mSelectionStart);
        assertEquals(2, mConnection.mSelectionEnd);
        assertEquals(0, mConnection.mCompositionStart);
        assertEquals(2, mConnection.mCompositionEnd);

        mImeAdapter.checkCompositionQueueAndCallNative("hel", 1, false);
        assertWaitForSetEditableCallback(4, mConnection);
        assertEquals("hel", mConnection.mText);
        assertEquals(3, mConnection.mSelectionStart);
        assertEquals(3, mConnection.mSelectionEnd);
        assertEquals(0, mConnection.mCompositionStart);
        assertEquals(3, mConnection.mCompositionEnd);

        mImeAdapter.checkCompositionQueueAndCallNative("hel", 1, true);
        assertWaitForSetEditableCallback(5, mConnection);
        assertEquals("hel", mConnection.mText);
        assertEquals(3, mConnection.mSelectionStart);
        assertEquals(3, mConnection.mSelectionEnd);
        assertEquals(-1, mConnection.mCompositionStart);
        assertEquals(-1, mConnection.mCompositionEnd);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeCopy() throws Exception {
        mImeAdapter.checkCompositionQueueAndCallNative("hello", 1, true);
        assertWaitForSetEditableCallback(2, mConnection);
        assertEquals("hello", mConnection.mText);
        assertEquals(5, mConnection.mSelectionStart);
        assertEquals(5, mConnection.mSelectionEnd);

        mImeAdapter.setEditableSelectionOffsets(2, 5);
        assertWaitForSetEditableCallback(3, mConnection);
        assertEquals("hello", mConnection.mText);
        assertEquals(2, mConnection.mSelectionStart);
        assertEquals(5, mConnection.mSelectionEnd);

        mImeAdapter.copy();
        assertClipboardContents(getActivity(), "llo");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeCut() throws Exception {
        mImeAdapter.checkCompositionQueueAndCallNative("snarful", 1, true);
        assertWaitForSetEditableCallback(2, mConnection);
        assertEquals("snarful", mConnection.mText);
        assertEquals(7, mConnection.mSelectionStart);
        assertEquals(7, mConnection.mSelectionEnd);

        mImeAdapter.setEditableSelectionOffsets(1, 5);
        assertWaitForSetEditableCallback(3, mConnection);
        assertEquals("snarful", mConnection.mText);
        assertEquals(1, mConnection.mSelectionStart);
        assertEquals(5, mConnection.mSelectionEnd);

        mImeAdapter.cut();
        assertWaitForSetEditableCallback(4, mConnection);
        assertEquals("sul", mConnection.mText);
        assertEquals(1, mConnection.mSelectionStart);
        assertEquals(1, mConnection.mSelectionEnd);

        assertClipboardContents(getActivity(), "narf");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImePaste() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ClipboardManager clipboardManager =
                        (ClipboardManager) getActivity().getSystemService(
                                Context.CLIPBOARD_SERVICE);
                clipboardManager.setPrimaryClip(ClipData.newPlainText("blarg", "blarg"));
            }
        });

        mImeAdapter.paste();
        assertWaitForSetEditableCallback(2, mConnection);
        assertEquals("blarg", mConnection.mText);
        assertEquals(5, mConnection.mSelectionStart);
        assertEquals(5, mConnection.mSelectionEnd);

        mImeAdapter.setEditableSelectionOffsets(3, 5);
        assertWaitForSetEditableCallback(3, mConnection);
        assertEquals("blarg", mConnection.mText);
        assertEquals(3, mConnection.mSelectionStart);
        assertEquals(5, mConnection.mSelectionEnd);

        mImeAdapter.paste();
        assertWaitForSetEditableCallback(5, mConnection);
        assertEquals("blablarg", mConnection.mText);
        assertEquals(8, mConnection.mSelectionStart);
        assertEquals(8, mConnection.mSelectionEnd);

        mImeAdapter.paste();
        assertWaitForSetEditableCallback(6, mConnection);
        assertEquals("blablargblarg", mConnection.mText);
        assertEquals(13, mConnection.mSelectionStart);
        assertEquals(13, mConnection.mSelectionEnd);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeSelectAndUnSelectAll() throws Exception {
        mImeAdapter.checkCompositionQueueAndCallNative("hello", 1, true);
        assertWaitForSetEditableCallback(2, mConnection);
        assertEquals("hello", mConnection.mText);
        assertEquals(5, mConnection.mSelectionStart);
        assertEquals(5, mConnection.mSelectionEnd);

        mImeAdapter.selectAll();
        assertWaitForSetEditableCallback(3, mConnection);
        assertEquals("hello", mConnection.mText);
        assertEquals(0, mConnection.mSelectionStart);
        assertEquals(5, mConnection.mSelectionEnd);

        mImeAdapter.unselect();
        assertWaitForSetEditableCallback(4, mConnection);
        assertEquals("", mConnection.mText);
        assertEquals(0, mConnection.mSelectionStart);
        assertEquals(0, mConnection.mSelectionEnd);

        assertWaitForKeyboardStatus(false);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testUpdatesGetIgnoredDuringBatchEdits() throws Throwable {
        mConnection.beginBatchEdit();
        assertWaitForSetIgnoreUpdates(true, mConnection);

        mImeAdapter.checkCompositionQueueAndCallNative("h", 1, false);
        assertWaitForSetEditableCallback(2, mConnection);
        assertEquals("h", mConnection.mText);
        assertEquals(1, mConnection.mSelectionStart);
        assertEquals(1, mConnection.mSelectionEnd);
        assertEquals(0, mConnection.mCompositionStart);
        assertEquals(1, mConnection.mCompositionEnd);
        assertTrue(mConnection.isIgnoringTextInputStateUpdates());

        mImeAdapter.checkCompositionQueueAndCallNative("he", 1, false);
        assertWaitForSetEditableCallback(3, mConnection);
        assertEquals("he", mConnection.mText);
        assertEquals(2, mConnection.mSelectionStart);
        assertEquals(2, mConnection.mSelectionEnd);
        assertEquals(0, mConnection.mCompositionStart);
        assertEquals(2, mConnection.mCompositionEnd);
        assertTrue(mConnection.isIgnoringTextInputStateUpdates());

        mImeAdapter.checkCompositionQueueAndCallNative("hel", 1, false);
        assertWaitForSetEditableCallback(4, mConnection);
        assertEquals("hel", mConnection.mText);
        assertEquals(3, mConnection.mSelectionStart);
        assertEquals(3, mConnection.mSelectionEnd);
        assertEquals(0, mConnection.mCompositionStart);
        assertEquals(3, mConnection.mCompositionEnd);

        assertEquals(0, mConnection.mUpdateSelectionCounter);
        assertTrue(mConnection.isIgnoringTextInputStateUpdates());
        mConnection.endBatchEdit();
        assertWaitForSetIgnoreUpdates(false, mConnection);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testShowImeIfNeeded() throws Throwable {
        DOMUtils.focusNode(this, mContentView, mCallbackContainer, "input_radio");
        assertWaitForKeyboardStatus(false);

        performShowImeIfNeeded();
        assertWaitForKeyboardStatus(false);

        DOMUtils.focusNode(this, mContentView, mCallbackContainer, "input_text");
        assertWaitForKeyboardStatus(false);

        performShowImeIfNeeded();
        assertWaitForKeyboardStatus(true);
    }

    private void performShowImeIfNeeded() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mContentView.getContentViewCore().showImeIfNeeded();
            }
        });
    }

    private void performGo(final AdapterInputConnection inputConnection,
            TestCallbackHelperContainer testCallbackHelperContainer) throws Throwable {
        handleBlockingCallbackAction(
                testCallbackHelperContainer.getOnPageFinishedHelper(),
                new Runnable() {
                    @Override
                    public void run() {
                        inputConnection.performEditorAction(EditorInfo.IME_ACTION_GO);
                    }
                });
    }



    private void assertWaitForPageScaleFactor(final float scale) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getContentViewCore().getScale() == scale;
            }
        }));
    }

    private void assertWaitForKeyboardStatus(final boolean show) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return show == getImeAdapter().mIsShowWithoutHideOutstanding &&
                        (!show || getAdapterInputConnection() != null);
            }
        }));
    }

    private void assertWaitForSetEditableCallback(final int callbackNumber,
            final TestAdapterInputConnection connection) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return callbackNumber == connection.mSetEditableTextCallCounter;
            }
        }));
    }

    private void assertClipboardContents(final Activity activity, final String expectedContents)
            throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        ClipboardManager clipboardManager =
                                (ClipboardManager) activity.getSystemService(
                                        Context.CLIPBOARD_SERVICE);
                        ClipData clip = clipboardManager.getPrimaryClip();
                        return clip != null && clip.getItemCount() == 1
                                && TextUtils.equals(clip.getItemAt(0).getText(), expectedContents);
                    }
                });
            }
        }));
    }

    private void assertWaitForSetIgnoreUpdates(final boolean ignore,
            final TestAdapterInputConnection connection) throws Throwable {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ignore == connection.isIgnoringTextInputStateUpdates();
            }
        }));
    }

    private ImeAdapter getImeAdapter() {
        return getContentViewCore().getImeAdapterForTest();
    }

    private ImeAdapter.AdapterInputConnection getAdapterInputConnection() {
        return getContentViewCore().getInputConnectionForTest();
    }

    private static class TestAdapterInputConnectionFactory extends
            ImeAdapter.AdapterInputConnectionFactory {
        @Override
        public AdapterInputConnection get(View view, ImeAdapter imeAdapter,
                EditorInfo outAttrs) {
            return new TestAdapterInputConnection(view, imeAdapter, outAttrs);
        }
    }

    private static class TestAdapterInputConnection extends ImeAdapter.AdapterInputConnection {
        private int mSetEditableTextCallCounter = 0;
        private int mUpdateSelectionCounter = 0;
        private String mText;
        private int mSelectionStart = INVALID_SELECTION;
        private int mSelectionEnd = INVALID_SELECTION;
        private int mCompositionStart = INVALID_COMPOSITION;
        private int mCompositionEnd = INVALID_COMPOSITION;

        public TestAdapterInputConnection(View view, ImeAdapter imeAdapter, EditorInfo outAttrs) {
            super(view, imeAdapter, outAttrs);
        }

        @Override
        public void setEditableText(String text, int selectionStart, int selectionEnd,
                int compositionStart, int compositionEnd) {
            mText = text;
            mSelectionStart = selectionStart;
            mSelectionEnd = selectionEnd;
            mCompositionStart = compositionStart;
            mCompositionEnd = compositionEnd;
            mSetEditableTextCallCounter++;
            super.setEditableText(
                    text, selectionStart, selectionEnd, compositionStart, compositionEnd);
        }

        @Override
        protected void updateSelection(
                int selectionStart, int selectionEnd,
                int compositionStart, int compositionEnd) {
            mUpdateSelectionCounter++;
        }
    }

    private static class TestInputMethodManagerWrapper extends InputMethodManagerWrapper {
        private ContentViewCore mContentViewCore;
        private InputConnection mInputConnection;

        public TestInputMethodManagerWrapper(ContentViewCore contentViewCore) {
            super(null);
            mContentViewCore = contentViewCore;
        }

        @Override
        public void restartInput(View view) {
            mInputConnection = mContentViewCore.onCreateInputConnection(new EditorInfo());
        }

        @Override
        public void showSoftInput(View view, int flags, ResultReceiver resultReceiver) {
            if (mInputConnection != null) return;
            mInputConnection = mContentViewCore.onCreateInputConnection(new EditorInfo());
        }

        @Override
        public boolean isActive(View view) {
            if (mInputConnection == null) return false;
            return true;
        }

        @Override
        public boolean hideSoftInputFromWindow(IBinder windowToken, int flags,
                ResultReceiver resultReceiver) {
            boolean retVal = mInputConnection == null;
            mInputConnection = null;
            return retVal;
        }

        @Override
        public void updateSelection(View view, int selStart, int selEnd,
                int candidatesStart, int candidatesEnd) {
        }
    }
}
