// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(() => {
  /**
   * Check if |value| equals the |desiredValue| within 1% margin of tolerance.
   * @param {number} value The variable value.
   * @param {number} desiredValue The desired value.
   * @return {boolean}
   */
  function equal1PercentMargin(value, desiredValue) {
    // floor and ceil to account to at least +/-1 unit.
    const minValue = Math.floor(desiredValue * 0.99);
    const maxValue = Math.ceil(desiredValue * 1.01);

    const result =
        value === desiredValue || (minValue <= value && value <= maxValue);
    if (!result) {
      console.log(
          'min value: ' + minValue + ' got value: ' + value +
          ' max value: ' + maxValue);
    }

    return result;
  }

  /**
   * Creates a test file, which can be inside folders, however parent folders
   * have to be created by the caller using |createTestFolder|.
   * @param {string} path File path to be created,
   * @return {TestEntryInfo}
   */
  function createTestFile(path) {
    const name = path.split('/').pop();
    return new TestEntryInfo({
      targetPath: path,
      nameText: name,
      type: EntryType.FILE,
      lastModifiedTime: 'Sep 4, 1998, 12:34 PM',
      sizeText: '51 bytes',
      typeText: 'Plain text',
      sourceFileName: 'text.txt',
      mimeType: 'text/plain'
    });
  }

  /**
   * Creates a test file, which can be inside another folder, however parent
   * folders have to be created by the caller.
   * @param {string} path Folder path to be created,
   * @return {TestEntryInfo}
   */
  function createTestFolder(path) {
    const name = path.split('/').pop();
    return new TestEntryInfo({
      targetPath: path,
      nameText: name,
      type: EntryType.DIRECTORY,
      lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
      sizeText: '--',
      typeText: 'Folder'
    });
  }

  /**
   * Creates a Shared Drive.
   * @param {string} name Shared Drive name.
   * @return {TestEntryInfo}
   */
  function createTestTeamDrive(name) {
    return new TestEntryInfo({
      teamDriveName: name,
      type: EntryType.SHARED_DRIVE,
      capabilites: {
        canCopy: true,
        canDelete: true,
        canRename: true,
        canAddChildren: true,
        canShare: true,
      },
    });
  }

  /**
   * Entries used by Drive and Downloads tests.
   * @type {!Array<TestEntryInfo>}
   */
  const testEntries = [
    createTestFile('file1.txt'),
    createTestFile('file2.txt'),
    createTestFile('file3.txt'),
    createTestFile('file4.txt'),
    createTestFile('file5.txt'),
    createTestFile('file6.txt'),
    createTestFile('file7.txt'),
    createTestFile('file8.txt'),

    createTestFolder('photos1'),
    createTestFolder('photos1/folder1'),
    createTestFolder('photos1/folder2'),
    createTestFile('photos1/file1.txt'),

    createTestFolder('photos2'),
    createTestFolder('photos3'),
  ];

  /**
   * Measures the number of metadata operations generated for:
   *  - Opening Files app in My Drive with 8 files and 3 folders.
   *  - Navigate to My Drive > photos1 > folder2, which is empty.
   */
  testcase.metadataDrive = async () => {
    // Open Files app on Drive.
    const appId =
        await setupAndWaitUntilReady(RootPath.DRIVE, testEntries, testEntries);

    // Navigate 2 folders deep, because navigating in directory tree might
    // trigger further metadata fetches.
    await remoteCall.navigateWithDirectoryTree(
        appId, '/root/photos1/folder1', 'My Drive', 'drive');

    // Fetch the metadata stats.
    const metadataStats =
        await remoteCall.callRemoteTestUtil('getMetadataStats', appId, []);

    // Verify the number of metadata operations generated by the whole
    // navigation above.
    // If the asserts below fail, check if your change has increased the number
    // of metadata operations, because they impact the overall app performance.
    // Full fetch tally:
    //    8 files in My Drive
    // +  3 folders in My Drive.
    // +  1 My Drive root.
    // +  2 folders when expanding photos1
    // = 14
    chrome.test.assertEq(14, metadataStats.fullFetch);
    chrome.test.assertEq(12, metadataStats.fromCache);

    // Cleared 8 files + 3 folders when navigated out of My Drive and
    // clearing file list.
    chrome.test.assertEq(11, metadataStats.clearCacheCount);
    chrome.test.assertEq(0, metadataStats.clearAllCount);
    chrome.test.assertEq(0, metadataStats.invalidateCount);
  };

  /**
   * Measures the number of metadata operations generated for:
   *  - Opening Files app in Downloads with 8 files and 3 folders.
   *  - Navigate to Downloads > photos1 > folder1 which is empty.
   */
  testcase.metadataDownloads = async () => {
    // Open Files app on Downloads.
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, testEntries, testEntries);

    // Navigate 2 folders deep, because navigating in directory tree might
    // triggers further metadata fetches.
    await remoteCall.navigateWithDirectoryTree(
        appId, '/Downloads/photos1/folder1', 'My files');

    // Fetch the metadata stats.
    const metadataStats =
        await remoteCall.callRemoteTestUtil('getMetadataStats', appId, []);

    // Verify the number of metadata operations generated by the whole
    // navigation above.
    // If the asserts below fail, check if your change has increased the number
    // of metadata operations, because they impact the overall app performance.
    // Full fetch tally:
    //    8 files in Downloads
    // +  3 folders in Downloads.
    // +  1 Downloads root.
    // +  1 read again photos1 when naviated to it.
    // +  8 files in Downloads again.
    // = 21
    chrome.test.assertEq(21, metadataStats.fullFetch);

    // 8 files and 3 folders in Downloads when expanding in the directory
    // tree.
    chrome.test.assertEq(0, metadataStats.fromCache);

    // Cleared 8 files + 3 folders when navigated out of Downloads and
    // clearing file list.
    chrome.test.assertEq(11, metadataStats.clearCacheCount);
    chrome.test.assertEq(0, metadataStats.clearAllCount);
    chrome.test.assertEq(0, metadataStats.invalidateCount);
  };

  /**
   * Measures the number of metadata operations generated for:
   *  - Opening Files app in My Drive with 51 folders.
   *  - Navigate to My Drive > folder1 which has 50 files.
   *
   * Using 50 files and 50 folders because in the Drive backend it has a
   * throttle for max of 20 concurrent operations.
   */
  testcase.metadataLargeDrive = async () => {
    const entries = [createTestFolder('folder1')];

    const folder1ExpectedRows = [];

    for (let i = 0; i < 50; i++) {
      const testFile = createTestFile('folder1/file-' + i + '.txt');
      entries.push(testFile);
      folder1ExpectedRows.push(testFile.getExpectedRow());

      // Using sibling folders because navigateWithDirectoryTree expands their
      // parent directory tree might issue metadata requests the child folders.
      entries.push(createTestFile('sibling-folder-' + i));
    }

    // Open Files app on Drive.
    const appId =
        await setupAndWaitUntilReady(RootPath.DRIVE, entries, entries);
    console.log('setupAndWaitUntilReady finished!');

    // Navigate only 1 folder deep,which is slightly different from
    // metadatatDrive test.
    await remoteCall.navigateWithDirectoryTree(
        appId, '/root/folder1', 'My Drive', 'drive');

    // Wait for the metadata stats to reach the desired count.
    // File list component, doesn't display all files at once for performance
    // reasons. Since we can't check the modifiedTime for all files in file
    // list, which is a proxy for "all metadata requests have finished", we have
    // to wait until the metadata stats to have the expected count.
    // If the asserts below fail, check if your change has increased the number
    // of metadata operations, because they impact the overall app performance.
    const checkMetadata = (metadataStats) => {
      let result = true;
      // Full fetch tally:
      //    51 files in My Drive.
      // +  50 files in My Drive>folder1.
      // +   1 My Drive root.
      // +   1 read again folder1 when naviated to it.
      // = 103
      result &= equal1PercentMargin(metadataStats.fullFetch, 103);

      // 50 team drives cached, reading from file list when navigating to
      // /team_drives, then read cached when expanding directory tree.
      result &= metadataStats.fromCache < 70;

      // Cleared 51 folders when navigated out of My Drive and clearing file
      // list.
      result &= equal1PercentMargin(metadataStats.clearCacheCount, 51);
      result &= metadataStats.clearAllCount === 0;
      result &= metadataStats.invalidateCount === 0;
      return result;
    };
    await remoteCall.waitFor('getMetadataStats', appId, checkMetadata);
  };

  /**
   * Measures the number of metadata operations generated for:
   *  - Opening Files app in My Drive, with 50 folders and 50 files.
   *  - Navigate to Shared Drives, with 50 team drives.
   *  - Expand Shared Drives to display the 50 team drives..
   */
  testcase.metadataTeamDrives = async () => {
    const entries = [];
    const driveEntries = [];

    // Using 50 files and 50 folders because in the Drive backend it has a
    // throttle for max of 20 concurrent jobs.
    for (let i = 0; i < 50; i++) {
      entries.push(createTestFile('file-' + i + '.txt'));
      // Using sibling folders because navigateWithDirectoryTree expands their
      // parent and directory_tree.js issues some metadata requests for this
      // condition.
      entries.push(createTestFolder('sibling-folder-' + i));
      driveEntries.push(createTestTeamDrive('Team Drive ' + i));
    }

    // Downloads just some entries. Load some in Downloads and Downloads to
    // check that Downloads entries don't issue metadata calls when navigating
    // on Drive/Shared drives.
    const downloadsEntries = entries.slice(0, 7);

    const sharedDrivesTreeItem =
        '#directory-tree [entry-label="Shared drives"]';

    // Open Files app on Drive.
    const appId = await setupAndWaitUntilReady(
        RootPath.DRIVE, downloadsEntries, entries.concat(driveEntries));

    // Navigate to Shared drives root.
    await remoteCall.navigateWithDirectoryTree(
        appId, '/team_drives', 'Shared drives', 'drive');

    // Expand Shared Drives, because expanding might need metadata.
    const expandIcon = sharedDrivesTreeItem + ' > .tree-row .expand-icon';
    await remoteCall.waitForElement(appId, expandIcon);

    // Click expand icon.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeMouseClick', appId, [expandIcon]));

    // Wait for the subtree to expand and display its children.
    const expandedSubItems =
        sharedDrivesTreeItem + ' > .tree-children[expanded] > .tree-item';
    await remoteCall.waitForElement(appId, expandedSubItems);

    // Get all Shared Drives' children.
    const elements = await remoteCall.callRemoteTestUtil(
        'queryAllElements', appId,
        [sharedDrivesTreeItem + ' > .tree-children[expanded] > .tree-item']);

    // Check that we have 50 team drives.
    chrome.test.assertEq(50, elements.length);

    // Fetch the metadata stats.
    const metadataStats =
        await remoteCall.callRemoteTestUtil('getMetadataStats', appId, []);

    // Verify the number of metadata operations generated by the whole
    // navigation above.
    // If the asserts below fail, check if your change has increased the number
    // of metadata operations, because they impact the overall app performance.
    //
    // Full fetch tally:
    //    50 files in My Drive.
    // +  50 folders in My Drive.
    // +  50 team drives.
    // +   1 My Drive root.
    // +   1 Shared Drives root.
    // = 152
    chrome.test.assertEq(152, metadataStats.fullFetch);

    // 50 team drives cached, reading from file list when navigating to
    // /team_drives, then read cached when expanding directory tree.
    chrome.test.assertEq(50, metadataStats.fromCache);

    // Cleared 50 folders + 50 files when navigated out of My Drive and
    // clearing file list.
    chrome.test.assertEq(100, metadataStats.clearCacheCount);
    chrome.test.assertEq(0, metadataStats.clearAllCount);
    chrome.test.assertEq(0, metadataStats.invalidateCount);
  };

  /**
   *  Tests that fetching content metadata from a DocumentsProvider completes.
   */
  testcase.metadataDocumentsProvider = async () => {
    const documentsProviderVolumeQuery =
        '[has-children="true"] [volume-type-icon="documents_provider"]';

    // Open Files app.
    const appId = await openNewWindow(RootPath.DOWNLOADS);

    // Add files to the DocumentsProvider volume.
    await addEntries(['documents_provider'], BASIC_LOCAL_ENTRY_SET);

    // Wait for the DocumentsProvider volume to mount.
    await remoteCall.waitForElement(appId, documentsProviderVolumeQuery);

    // Click to open the DocumentsProvider volume.
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil(
            'fakeMouseClick', appId, [documentsProviderVolumeQuery]),
        'fakeMouseClick failed');

    // Check: the DocumentsProvider files should appear in the file list.
    const files = TestEntryInfo.getExpectedRows(BASIC_LOCAL_ENTRY_SET);
    await remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true});

    // Select file hello.txt in the file list.
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil(
            'selectFile', appId, [ENTRIES.hello.nameText]),
        'selectFile failed');

    // Check that a request for content metadata completes.
    const result = await await remoteCall.callRemoteTestUtil(
        'getContentMetadata', appId, [['mediaMimeType']]);

    // Check nothing in the result was returned.
    chrome.test.checkDeepEq([], result);
  };
})();
