// The raw core structs -> public types, without the addon. Must match the React Native
// binding's C++ mapping (HybridEcr17Client.cpp).
import assert from "node:assert/strict";
import { describe, test } from "node:test";

import {
	mapClose,
	mapPayment,
	mapPreAuth,
	mapStatus,
	mapTotals,
	parseTerminalDateTime,
} from "../src/mappers.ts";
import type { RawPayment } from "../src/native.ts";

const rawPayment = (overrides: Partial<RawPayment> = {}): RawPayment => ({
	outcome: "ok",
	resultCode: "00",
	pan: "",
	transactionType: "",
	authCode: "",
	hostDateTime: "",
	errorDescription: "",
	cardType: "",
	acquirerId: "",
	stan: "",
	onlineId: "",
	currency: {
		applied: false,
		rate: "",
		currencyCode: "",
		amount: "",
		precision: "",
	},
	...overrides,
});

describe("mappers", () => {
	test("empty fields are omitted, not undefined", () => {
		const result = mapPayment(rawPayment());
		assert.deepEqual(result, { outcome: "ok", resultCode: "00" });
		assert.deepEqual(Object.keys(result), ["outcome", "resultCode"]);
	});

	test("DCC: numbers are not rescaled, as on React Native", () => {
		const result = mapPayment(
			rawPayment({
				currency: {
					applied: true,
					rate: "01234567",
					currencyCode: "USD",
					amount: "000000000710",
					precision: "2",
				},
			}),
		);
		assert.deepEqual(result.currencyExchange, {
			applied: true,
			rate: 1234567,
			currencyCode: "USD",
			amountCents: 710,
			precision: 2,
		});
	});

	test("DCC block is omitted when not applied", () => {
		assert.equal("currencyExchange" in mapPayment(rawPayment()), false);
	});

	test("entry modes and card types", () => {
		const modes = ["ICC", "MAG", "MAN", "CLM", "CLI", "???"].map(
			(transactionType) =>
				mapPayment(rawPayment({ transactionType })).entryMode,
		);
		assert.deepEqual(modes, [
			"icc",
			"mag",
			"manual",
			"clessMag",
			"clessIcc",
			undefined,
		]);
		const cards = ["1", "2", "3", "9"].map(
			(cardType) => mapPayment(rawPayment({ cardType })).cardType,
		);
		assert.deepEqual(cards, ["debit", "credit", "other", undefined]);
	});

	test("pre-auth amounts are cents", () => {
		const result = mapPreAuth({
			...rawPayment(),
			preAuthorizedAmount: "00012345",
			preAuthCode: "123456789",
			actionCode: "",
		});
		assert.equal(result.preAuthorizedAmountCents, 12345);
		assert.equal(result.preAuthCode, "123456789");
	});

	test("totals default to 0, close totals are optional", () => {
		assert.equal(
			mapTotals({ outcome: "ok", resultCode: "00", posTotal: "" })
				.posTotalCents,
			0,
		);
		assert.deepEqual(
			mapClose({
				outcome: "ok",
				resultCode: "00",
				posTotal: "0000000000001500",
				hostTotal: "",
				errorDescription: "",
				actionCode: "",
			}),
			{ outcome: "ok", resultCode: "00", posTotalCents: 1500 },
		);
	});

	test("terminal date: DDMMYYhhmm in local time, epoch when malformed", () => {
		assert.deepEqual(
			parseTerminalDateTime("3112251159"),
			new Date(2025, 11, 31, 11, 59),
		);
		assert.deepEqual(parseTerminalDateTime(""), new Date(0));
		assert.deepEqual(parseTerminalDateTime("ab12251159"), new Date(0));
	});

	test("an unknown status maps to -1", () => {
		const status = mapStatus({
			terminalId: "1",
			dateTimeRaw: "",
			status: -1,
			softwareRelease: "",
		});
		assert.equal(status.status, -1);
	});
});
