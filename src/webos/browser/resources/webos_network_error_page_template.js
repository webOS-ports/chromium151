// Copyright 2018 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

// Fills in webos_network_error_page.html and adds its buttons. The renderer
// defines, ahead of this script: errorCode (the net error, e.g. -105),
// appResolutionWidth / appResolutionHeight (unused here: the stylesheet is
// responsive) and error_info, the strings from GetErrorStrings() and
// GetWebOSLocalizedErrorStrings().
//
// Layout and sizing are left to the stylesheet. The buttons are real <button>s,
// so touch, a pointer and the keyboard (Tab, Enter, Space) all work without the
// TV remote focus handling the LG page carried.

var err_info = JSON.parse(JSON.stringify(error_info));

var isRTL = err_info.textdirection === "rtl";

var TargetEnum = {NETWORK: 1, GENERAL: 2};

/* Handle webOSRelaunch event: a web app relaunched while it shows this page
 * restarts, so the relaunch reloads it. */
function relaunchHandler(e) {
  if (!window.PalmSystem) {
    return;
  }
  function onclosecallback() {
    var param = {};
    var q_param = {};
    var serviceCall_url = "luna://com.webos.applicationManager/launch";
    param["id"] = PalmSystem.identifier;
    q_param["query"] = e.detail.query;
    param["params"] = q_param;
    PalmSystem.serviceCall(serviceCall_url, param);
  }
  PalmSystem.onclose = onclosecallback;
  window.close();
}
document.addEventListener('webOSRelaunch', relaunchHandler);

/* The strings are written in capitals for the TV's headers and buttons
 * ("CHECK NETWORK CONNECTION", "EXIT APP"). The Enyo look uses normal case.
 * Only English is recased: lower-casing a translation can be wrong (German
 * capitalises nouns), so other languages are shown as translated. */
function isEnglish() {
  return /^en(-|$)/i.test(navigator.language || "");
}

function isAllCaps(s) {
  return s.toUpperCase() === s && s.toLowerCase() !== s;
}

function sentenceCase(s) {
  if (!s || !isEnglish() || !isAllCaps(s)) {
    return s;
  }
  var lower = s.toLowerCase();
  return lower.charAt(0).toUpperCase() + lower.slice(1);
}

function titleCase(s) {
  if (!s || !isEnglish() || !isAllCaps(s)) {
    return s;
  }
  return s.toLowerCase().replace(/(^|\s)(\S)/g, function(m, sp, c) {
    return sp + c.toUpperCase();
  });
}

/* error_title carries the code in brackets ("CHECK NETWORK CONNECTION(-105)");
 * the header shows the code on its own, so take it off the title. */
function stripCode(title) {
  return (title || "").replace(/\s*\(\s*-?\d+\s*\)\s*$/, "");
}

var onExitApp = function() {
  if (window.errorPageController) {
    errorPageController.closeShellIfNeeded();
  }
  window.close();
};

function getDisplayId() {
  try {
    return JSON.parse(window.webOSSystem.launchParams).displayAffinity;
  } catch (e) {
    return undefined;
  }
}

function onLaunchSetting(target) {
  if (window.errorPageController) {
    errorPageController.settingsButtonClick(target, getDisplayId());
  }
}

var onLaunchNetworkSetting = function() {
  onLaunchSetting(TargetEnum.NETWORK);
};

var onLaunchGeneralSetting = function() {
  onLaunchSetting(TargetEnum.GENERAL);
};

/* Retry even when the network is down (-106): the user may just have
 * reconnected, and a retry that fails again lands back on this page. */
var onRetryApp = function() {
  window.location = err_info.url_to_reload;
};

function getCurrentTime() {
  var now = new Date();

  function twoDigits(d) {
    return d < 10 ? "0" + d : "" + d;
  }

  var currentTime = now.getFullYear() + '-' + twoDigits(now.getMonth() + 1) +
      '-' + twoDigits(now.getDate());
  currentTime = currentTime + "\u0009" + now.toTimeString();

  return currentTime + "‎";
}

/* Which buttons an error gets. The cases are the LG page's. */
function getLayoutCase(error) {
  switch (error) {
    case -109:    // ADDRESS_UNREACHABLE
      return 0;   // Exit
    case -105:    // NAME_NOT_RESOLVED
    case -106:    // INTERNET_DISCONNECTED
    case -137:    // NAME_RESOLUTION_FAILED
      return 2;   // Retry, Network settings, Exit
    case -201:    // CERT_DATE_INVALID
      return 3;   // Retry, Settings, Exit
    default:
      return 1;   // Retry, Exit
  }
}

function addButton(text, id, isPrimary, func) {
  var btn = document.createElement("button");
  btn.type = "button";
  btn.id = id;
  btn.className = isPrimary ? "button primary" : "button";
  btn.textContent = titleCase(text);
  btn.addEventListener("click", func);
  document.getElementById("Button_Container").appendChild(btn);
  return btn;
}

/* Buttons go in reading order of importance: the suggested action first. The
 * stylesheet stacks them on a narrow screen and lays a wide screen's out in
 * one row, suggested action at the end, as Enyo dialogs do. */
function setButtonsOnPage(layoutCase) {
  var first = null;
  if (layoutCase !== 0) {
    first = addButton(err_info.retry_button_text, 'Retry_Button', true,
                      onRetryApp);
  }
  if (layoutCase === 2) {
    addButton(err_info.network_settings_button_text, 'NetworkSetting_Button',
              false, onLaunchNetworkSetting);
  } else if (layoutCase === 3) {
    addButton(err_info.settings_button_text, 'Setting_Button', false,
              onLaunchGeneralSetting);
  }
  var exit = addButton(err_info.exit_app_button_text, 'ExitApp_Button',
                       layoutCase === 0, onExitApp);
  return first || exit;
}

function setText(id, text) {
  document.getElementById(id).textContent = text || "";
}

function setErrorPageMessages(error_title, error_details, error_guide,
                              error_info_text) {
  setText("errortitle", sentenceCase(stripCode(error_title)));
  setText("errordetails", error_details);
  setText("errorguide", error_guide);
  setText("errorinfo", error_info_text);
}

function onload() {
  document.documentElement.dir = isRTL ? "rtl" : "ltr";
  if (navigator.language) {
    document.documentElement.lang = navigator.language;
  }

  var info = err_info.error_info;
  if (errorCode == -201) {
    // A certificate that is not yet, or no longer, valid is often a wrong
    // clock: show the time the device has.
    info = (info ? info + ' : ' : '') + getCurrentTime();
  }
  setErrorPageMessages(err_info.error_title, err_info.error_details,
                       err_info.error_guide, info);
  setText("errorcode", String(errorCode));

  // Read the message out before the focus moves to a button.
  document.getElementById("error_header").setAttribute("role", "alert");

  var focusButton = setButtonsOnPage(getLayoutCase(errorCode));

  // With audio guidance on, start on the suggested button so it is read out
  // and Enter takes it. Without, leave the focus alone: a focused button on a
  // touch screen only draws a ring nobody asked for.
  function initFocus() {
    if (focusButton) {
      focusButton.focus();
    }
  }
  if (document.accessibilityEnabled) {
    if (document.accessibilityReady) {
      initFocus();
    } else {
      // The web engine is not yet ready to handle a focus event.
      document.addEventListener('webOSAccessibilityReady', initFocus, false);
    }
  }
}
