// Service worker for context menu and notifications

chrome.runtime.onInstalled.addListener(() => {
  chrome.contextMenus.create({
    id: 'vd-dub',
    title: '🎬 Dub with VideoDubber',
    contexts: ['video', 'link', 'page'],
    documentUrlPatterns: ['https://www.youtube.com/*']
  });
});

chrome.contextMenus.onClicked.addListener((info, tab) => {
  if (info.menuItemId === 'vd-dub') {
     chrome.tabs.sendMessage(tab.id, { type: 'START_DUB', url: tab.url });
  }
});
