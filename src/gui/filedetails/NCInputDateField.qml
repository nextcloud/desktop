/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

NCInputTextField {
    id: root

    function updateText() {
        text = _textFromValue(_value, root.locale);
    }

    // Taken from Kalendar 22.08
    // https://invent.kde.org/pim/kalendar/-/blob/release/22.08/src/contents/ui/KalendarUtils/dateutils.js
    function _parseDateString(dateString) {
        function defaultParse() {
            const defaultParsedDate = Date.fromLocaleDateString(root.locale, dateString, Locale.NarrowFormat);
            // JS always generates date in system locale, eliminate timezone difference to UTC
            const msecsSinceEpoch = defaultParsedDate.getTime() - (defaultParsedDate.getTimezoneOffset() * 60 * 1000);
            return new Date(msecsSinceEpoch);
        }

        const dateStringDelimiterMatches = dateString.match(/\D/);
        if(dateStringDelimiterMatches.length === 0) {
            // Let the date method figure out this weirdness
            return defaultParse();
        }

        const dateStringDelimiter = dateStringDelimiterMatches[0];

        const localisedDateFormatSplit = root.locale.dateFormat(Locale.NarrowFormat).split(dateStringDelimiter);
        const localisedDateDayPosition = localisedDateFormatSplit.findIndex((x) => /d/gi.test(x));
        const localisedDateMonthPosition = localisedDateFormatSplit.findIndex((x) => /m/gi.test(x));
        const localisedDateYearPosition = localisedDateFormatSplit.findIndex((x) => /y/gi.test(x));

        let splitDateString = dateString.split(dateStringDelimiter);
        let userProvidedYear = splitDateString[localisedDateYearPosition]

        const dateNow = new Date();
        const stringifiedCurrentYear = dateNow.getFullYear().toString();

        // If we have any input weirdness, or if we have a fully-written year
        // (e.g. 2022 instead of 22) then use default parse
        if(splitDateString.length === 0 ||
                splitDateString.length > 3 ||
                userProvidedYear.length >= stringifiedCurrentYear.length) {

            return defaultParse();
        }

        let fullyWrittenYear = userProvidedYear.split("");
        const digitsToAdd = stringifiedCurrentYear.length - fullyWrittenYear.length;
        for(let i = 0; i < digitsToAdd; i++) {
            fullyWrittenYear.splice(i, 0, stringifiedCurrentYear[i])
        }
        fullyWrittenYear = fullyWrittenYear.join("");

        const fixedYearNum = Number(fullyWrittenYear);
        const monthIndexNum = Number(splitDateString[localisedDateMonthPosition]) - 1;
        const dayNum = Number(splitDateString[localisedDateDayPosition]);

        console.log(dayNum, monthIndexNum, fixedYearNum);

        // Modification: return date in UTC
        return new Date(Date.UTC(fixedYearNum, monthIndexNum, dayNum));
    }

    function _textFromValue(value, locale) {
        const dateFromValue = new Date(value * dayInMSecs);
        return dateFromValue.toLocaleDateString(root.locale, Locale.NarrowFormat);
    }

    function _valueFromText(text, locale) {
        const dateFromText = _parseDateString(text);
        return Math.floor(dateFromText.getTime() / dayInMSecs);
    }

    property var date: new Date().getTime() * 1000 // QDateTime msecsFromEpoch
    onDateChanged: updateText()

    property var minimumDate: 0
    property var maximumDate: Number.MAX_SAFE_INTEGER

    validInput: {
        const value = valueFromText(text);
        return value >= minimumDate && value <= maximumDate;
    }

    text: _textFromValue(_value, locale)
    inputMethodHints: Qt.ImhDate

    onAccepted: root.date = _valueFromText(text, locale);
}