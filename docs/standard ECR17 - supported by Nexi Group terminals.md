---
title: "Traditional POS Start Page | Traditional POS | Nexi group developer portal"
source: "https://developer.nexigroup.com/traditionalpos/en-EU/docs/"
author:
published:
created: 2026-05-27
description:
tags:
  - "clippings"
---
## Documentation for Traditional POS

Traditional POS documentation includes the description of the protocol, derived from the Italian standard **ECR17**, supported by Nexi Group terminals for local connection with Merchant systems in order to integrate electronic payment services.

![undefined](https://developer.nexigroup.com/static/background-f6a3aae6cf2045cedb11ea0cce88701a.png)

Traditional terminals are offered in the following configurations:

- **Desktop with Pin Pad:** connected via Ethernet to the shop lan, can be used as a standalone terminal or integrated with the cash register.
- **Cordless+:** can be connected via Ethernet through a charge and connectivity cradle and directly via Wi-Fi, to the shop’s LAN, allowing the merchant to accept payments even outside the cradle's coverage range. Cordless terminals are suitable for integrated solutions at the checkout via LAN.
- **Portable:** equipped with a 4G SIM, allow the Merchant to accept payments in shop where there is no LAN option or outside. In this case, there is no integration option available and, if required, you must choose a SmartPOS solution.
- **Retail Pin Pad:** devices without printer that are connected via Ethernet to the shop LAN and always require an integration with the cash register.



-----------------------

---
title: "Traditional POS | Nexi group developer portal"
source: "https://developer.nexigroup.com/traditionalpos/en-EU/docs/integration-options-this-section-provides-an-overview-of-lan-integration-and-other-basic-integration-options/"
author:
published:
created: 2026-05-27
description:
tags:
  - "clippings"
---
## Integration options

## LAN integration

LAN integration allows the Merchant to activate the POS directly from the ECR, sending a message from the ECR to the IP address of the POS. For new EFTPOS based with Linux OS is required a port over 1024.

Some terminals have also USB/RS232 ports, but the usage is deprecated, as it might be not available on new products in the future.

The POS interacts with the Cardholder reading the payment card and send the authorization request to NEXI.

After the completion of the payment, the POS sends a response message to the ECR in synchronous way.

Is up to you to define if the POS prints the payment receipts at the end of the transaction or to integrate the payment receipts in the tax receipts printed by ECR.

## Integration test kit

The Merchant can request through his commercial contact (i.e. Bank, Nexi or ISV) the supply of a test kit composed by a terminal and a kit of cards, to test the integration made by the Developer.

If you have no commercial contact, you can ask to be contacted by NEXI filling the form at the following page: [Partner Program](https://www.nexi.it/it/partner-program)

## Go-live

The Merchant subscribes the POS and acquiring contract with Nexi through his commercial contact.

Nexi technical support team contacts the Merchant for checking the compatibility between ECR and POS, and for gathering all POS configuration need for the activation (IP, PORT, etc.).

You may require the following POS configuration:

- **ECR Integration and standalone usage** – Payments can be requested by the cash register o manually through the user interface of the POS terminal
- **ECR Integration only** – Payment can be requested only by the ECR When all the information is available, NEXI performs the configuration activity onsite or remotely.

## Basic integration

## Terminal Status

ECR verifies Eft-POS status by [Terminal Status](https://developer.nexigroup.com/traditionalpos/en-EU/docs/terminal-status-request-message-from-ecr/).

Terminal Status command guarantee the connection between ECR and EFT-POS.

## Payment

### Basic payment

ECR requests a Payment EFT-POS by command Payment, the payment amount is filled.

EFT-POS will process transaction and response with the transaction Result. For detail see [Payment](https://developer.nexigroup.com/traditionalpos/en-EU/docs/payment/) (command “P” or “X”).

Below is reported a few information useful to understand the protocol.

#### Date and time

**EFT-POS:**

- is synchronized with Host System
- Displays local data and time
- Via Lan Integration protocol, ECR receives the date and time of the Host System

**Payment Type:** The response field reports the type of card processed. For historical Reason:

- Debit Card is used to refer *PagoBANCOMAT* Card
- Credit Card is used to refer International Card

**Aquirer ID:** The response field reports Acquirer code that process transaction. Warning This Acquirer Id code may be different for each Scheme/Acquirer.

Below is shown the basic payment flow:

![Basic Payment Flow](https://images.ctfassets.net/di680may5c0k/6LrqNa2mtkkE69ZuO6A2bV/374e0f1652e0db69d527c8678f0ca22b/Basic_Payment_Flow.jpg?w=2500&h=1303&q=50&fm=webp)

### Payment with Receipt manage by ECR and Additional TAG

In this use case, it is required that the receipt is printed on the ECR and not on the POS, and with the execution of a Payment and simultaneous sending of additional TAGs to GT, no Addition TAGs received from GT.

LAN Integration requires managing of multiple commands.

The request consists of:

- instruct the POS on the receipt management mode (command E, management mode 1)
- the Payment function must be started by specifying the amount and advice of presence Additional TAGS (command “P”).
- data relating to Additional Tags (U command).

The response consists of:

- Transaction result via 'E' message (protocol document link)
- Receipt via Message/Messages 'S' (protocol document link)

Below is reported a few information useful to understand the protocol.

**Receipt**

- Receipt is formatted by the POS following the requirements of the Payment Schemes.
- The receipt, if requested by ECR, is sent pre-formatted.
- The ECR will print the receipt “as is”.

**Warning**

- NEXI can change the format of the receipt data without communication.
- Receipt could be transmitted by more than one ‘S’ message. It is up to ECR to concatenates all the ‘S’ messages.

In the below example, the receipt is sent through 2 'S' messages:

![Basic Payment Flow 2 -S- messages](https://images.ctfassets.net/di680may5c0k/5j8yo2Q04mJph8MXluxxrw/e8ca85561839bfccf334ecf96a2ca8d8/Basic_Payment_Flow_In_the_following_example__the_receipt_is_sent_through_2_-S-_messages.jpg?w=2500&h=2424&q=50&fm=webp)

### Exception Management

**Lost Payment Response**

- The exception can rise for different reason, i.e. connection drop, EFT-POS issues, ECR timeout, etc.
- The protocol provides the command “G” that can be used to retrieve last transaction result.

**Warning**

- It is not available unique identifier between the pending request with last transaction result.

**NAK usage**

- NAK is used to control the reception and retransmission of messages, a retransmission attempt is expected for a maximum of 3 times ([Communication Protocol](https://developer.nexigroup.com/traditionalpos/en-EU/docs/communication-protocol/))
- NAK is also returned in the case of commands not managed by POS.

**Reprint Receipt**

- ECR requests Reprint via command “R”.

## Reversal Last Transaction

Eft-Pos allows the cancellation of the last transaction performed. The purpose of the transaction is the management of any operational errors (i.e. incorrect amount entry).

The functionality requires reading the card data in the same way as the previous transaction.

ECR requests Reversal via command “S”.

**Warning** STAN management is recommended.

## Total and Close Session

EFT-POS records the totals of the transactions managed; also the Host records the totals. The Local and Remote Total are reset by Close Session operation.

ECR retrieved Local and Remote Total via command “T” putting in evidence any difference between locale and remote totals.

ECR requests a Close Session via command “C”.

## Functions for Hotel and Car rentals

For hotel and car rental, Nexi has a set of specific functions to allow optimal management of payment processes in compliance with the requirements of the payment schemes and the needs of the Merchant.

These functionalities are partially available using LAN Integration.

- **Preauthorization**: requested for the estimated value of the service in order to block the plafond of the Cardholder
- **Incremental Authorization**: requested to block an additional amount during the execution of the contracted service
- **Pre-authorization capture**: requested to debit the Cardholder for the final amount service.
- **Card verification**: requested to validate the card presented by the Cardholder.
- **No-Show (not available via LAN Integration)**: requested to debit the Cardholder for guaranteed reservation.
- **Delayed Charges (not available via LAN Integration)**: requested to debit the Cardholder after the payment of the original service (i.e. for fine o damages in car rental sector).
- **Pre-authorization void (not available via LAN Integration)**: requested to unblock the card plafond.
- **Reversal of last transaction**: requested for preauthorization request and capture.

Preauthorization requests deliver a unique code that can be used for the following requests (integration, capture, etc.) that can be requested by any POS of the Merchant.

## Preauthorization

Using LAN Integration, ECR initiates the preauthorization request with the estimated amount of the service.

The terminal processes the transaction and gives back the result with some identification data of the transaction.

In case of positive result, ECR receives the unique code of the preauthorization, instead, in case of negative result, a response code with the description of the error, if available.

Options and exceptions are the same of the payment.

For the details, please see command “p” specification in API section.

## Incremental Authorization

Using LAN Integration, ECR initiates the Incremental Authorization request using the unique preauthorization code received in the original preauthorization request and the additional estimated amount of the service.

For the details, please see command “i” specification in API section.

Options and exceptions are the same of the payment.

## Preauthorization Capture

Using LAN Integration, ECR initiates the Incremental Authorization request using the unique preauthorization code received in the original preauthorization request and the final amount of the service. For the details, please see command “c” specification in API section.

Options and exceptions are the same of the payment.

## Tokenization

Tokenization allows to save Cardholder’s payment data, after signing a debit mandate contract proposed by the Merchant, to manage the following use cases:

- Recurring Payments
- Unscheduled Recurring Payments
- OneClick Payments

Merchant can activate Tokenization only using LAN Integration with the following transactions:

- Payment
- Card Verification
- Preauthorization

Additional Tags must be valued, as described in API specification, by the ECR specifying a unique code that identify the contract signed by the Cardholder.

For the details, please see command “U” specification in API section. The subsequence transaction to debit the Cardholder are requested via API through XPAY Gateway.


-------------------------

---
title: "Traditional POS | Nexi group developer portal"
source: "https://developer.nexigroup.com/traditionalpos/en-EU/docs/lan-integration/"
author:
published:
created: 2026-05-27
description:
tags:
  - "clippings"
---
## LAN Integration

## Overview

Traditional POS supports LAN integration.

In this section, you will find the information for:

- [Communication Protocol](https://developer.nexigroup.com/traditionalpos/en-EU/docs/communication-protocol/)
- [Application Messages](https://developer.nexigroup.com/traditionalpos/en-EU/docs/terminal-status-request-message-from-ecr/)


------------------

---
title: "Traditional POS | Nexi group developer portal"
source: "https://developer.nexigroup.com/traditionalpos/en-EU/docs/communication-protocol/"
author:
published:
created: 2026-05-27
description:
tags:
  - "clippings"
---
## Communication Protocol

## Protocol rules and flow

Communication is always started by the ECR, which sends the transaction request message to the terminal. For payments and refunds, the message includes the transaction amount.

All messages are exchanged in plain text, and the characters composing the messages are not altered in any way.

All messages include a LRC control byte. The LRC character is computed by executing an exclusive or any message byte, using as base value 0x7F.

Both terminal and cash registers validate incoming messages. Validation may fail for one of these reasons:

- Protocol error (invalid message code, invalid terminal ID)
- Parity error
- LRC error

If validation fails, a NAK is sent to the ECR.

Both the terminal and the cash registers repeat the message transmission. It is repeated up to three, on these events:

- The wait message time-out expiration
- NAK received

If the function is enabled, the terminal will verify that the terminal ID specified in cash register messages is equal to the one sent to cash register, or 00000000.

## Application packet format

The format used for application packets is defined in the table below. These application packets refer to:

- Payment
- Reversal
- Refund
- Open session
- Close session
- Terminal status
- Send ticket
- Enable/disable ECR print
- Receipt reprint

This is possible due to all data being composed of application messages ASCII (0 to 127).

| Pos | Field name | Value | Type | Length (bytes) |
| --- | --- | --- | --- | --- |
| 1 | Start of text | 0x02 | Binary | 1 |
| 2…N | Application message |  | Binary | N |
| N+1 | End of Text | 0x03 | Binary | 1 |
| N+2 | LRC |  | Binary | 1 |

All the application-level packets have a physical confirmation message in response from the peer.

## Procedure progress update packet format

During the procedures that imply a connection to the host, the terminal can transmit a message containing information about the progress of the current procedure. The messages are 20 characters long so that they can be easily displayed on the ECR display. The progress update messages do not require the physical confirmation message from the cash register.

| Pos | Field name | Value | Type | Length (bytes) |
| --- | --- | --- | --- | --- |
| 1 | Start of heading | 0x01 | Binary | 1 |
| 2 | Message |  | Binary | 20 |
| 22 | End of transmission | 0x04 | Binary | 1 |

## Confirmation/refusal packet format

The confirmation/denial messages sent to the peer always have the following formats:

- **Confirmation message (ACK)**

| Pos | Field name | Value | Type | Length (bytes) |
| --- | --- | --- | --- | --- |
| 1 | ACK | 0x06 | Binary | 1 |
| 2 | ETX | 0x03 | Binary | 1 |
| 3 | LRC |  | Binary | 1 |

- **Refusal message (NAK)**

| Pos | Field name | Value | Type | Length (bytes) |
| --- | --- | --- | --- | --- |
| 1 | NAK | 0x15 | Binary | 1 |
| 2 | ETX | 0x03 | Binary | 1 |
| 3 | LRC |  | Binary | 1 |


---------------

---
title: "Terminal status request message (from ECR) | Traditional POS | Nexi group developer portal"
source: "https://developer.nexigroup.com/traditionalpos/en-EU/docs/terminal-status-request-message-from-ecr/"
author:
published:
created: 2026-05-27
description:
tags:
  - "clippings"
---
## Terminal status request message (from ECR)

## Terminal status request message (from ECR)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘s’ (0x73) |

## POS status response message (from Terminal)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | N | Message code: ‘s’ (0x73) |
| 11 | 10 | N | Reserved – fixed to ‘0’ (0x30) |
| 21 | 10 | N | Terminal date and time in format “DDMMYYhhmm |
| 31 | 1 | N | Terminal status - It depends on the terminals manufacturer. Es:  - '0' = Terminal not configured. - '1' = Terminal configured, no DLL. - '2' = Terminal operative (after a DLL). - '3' = Terminal not aligned (first DLL requested). - '4' = KMPB/KPOS key corrupted (first DLL requested). - '5' = DLL solicited by GT pending. - '6' = Remote SW updated request pending.  If the terminal ECR connection parameters are not configured, the command will not have response. |
| 32 | N\*8 | AN | Terminal SW release. It depends on the terminal’s manufacturer |


--------------------------
---
title: "Traditional POS | Nexi group developer portal"
source: "https://developer.nexigroup.com/traditionalpos/en-EU/docs/payment/"
author:
published:
created: 2026-05-27
description:
tags:
  - "clippings"
---
## Payment

## Payment request message (from ECR)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘P’ (0x50) |
| 11 | 8 | N | Cash register ID |
| 19 | 1 | N | Presence of message with additional data for the GT |
| 20 | 2 | N | Reserved – fixed to ‘0’ (0x30) |
| 22 | 1 | N | Start transaction when card already present:  - ‘0’ = start with card not yet inserted in the terminal - ‘1’ = start with card already inserted in the terminal |
| 23 | 1 | N | Payment type:  - ‘0’ = automatic card recognition on terminal with customer selection in case of cobranded cards - ‘1’ = only debit cards - ‘2’ = only credit cards - ‘3’ = other cards |
| 24 | 8 | N | Transaction amount, right aligned, filled with ‘0’ (0x30) on the left.  Amount is always considered to be expressed in cents (e.g. 650 = 6,50) |
| 32 | 128 | AN | Text to be print (Code contract) at the end of the payment receipt. The field is always 128 chars long, it’s right aligned, filled with ‘ ‘ (0x20) on the left. The terminal shall only print valid chars, skipping the blanks. |
| 160 | 8 | N | Reserved – fixed to ‘0’ (0x30) |

## Payment response message without currency exchange data (from Terminal)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘E’ (0x45) |
| 11 | 2 | N | Transaction result:  - “00” = OK - “01” = KO - “05” = card not present (valid only if field 22 in request message is ‘1’ = start with card already present - “09” = received unknown Tag from GT |

### If transaction result is positive (00)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 13 | 19 | N | Card PAN, right aligned, filled with ‘0’ (0x30) on the left. The PAN is |
| 32 | 3 | A | Transaction type:  - “ICC” = ICC card - “MAG” = magnetic stripe card - “MAN” = manual PAN entry - “CLM” = c-less magstripe - “CLI” = c-less ICC |
| 35 | 6 | A | Authorization code received from host |
| 41 | 7 | A | Transaction date and time received from host in DDDHHMM. The day is expressed as number of days since 01/01 of current year. For example, July 29, 15:20 is sent as 2111520. |

### If transaction result is negative (01)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 13 | 24 | A | Result description. This field shall report, using only ASCII characters, the reason of the denial. The field is left aligned, filled with blank on the right. |
| 37 | 11 | N | Reserved – fixed to ‘0’ (0x30) |

### Common to any response

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 48 | 1 | N | Card type:  - '1' = Bancomat card - '2' = Credit card - '3' = Other card |
| 49 | 11 | N | Acquirer ID. The field is left aligned, filled with blanks on the right. |
| 60 | 6 | N | STAN. Transaction sequence number. |
| 66 | 6 | N | ID online. Online operation progressive number. |

## Payment response message with currency exchange data (from Terminal)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘V’ (0x56) |
| 11 | 2 | N | Transaction result:  - “00” = OK - “01” = KO - “05” = card not present (valid only if field 22 in request message is ‘1’ = start with card already present - “09” = received unknown Tag from GT |

### If transaction result is positive (00)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 13 | 19 | N | Card PAN, right aligned, filled with ‘0’ (0x30) on the left. The PAN is |
| 32 | 3 | A | Transaction type:  - “ICC” = ICC card - “MAG” = magnetic stripe card - “MAN” = manual PAN entry - “CLM” = c-less magstripe - “CLI” = c-less ICC |
| 35 | 6 | A | Authorization code received from host |
| 41 | 7 | A | Transaction date and time received from host in DDDHHMM. The day is expressed as number of days since 01/01 of current year. For example, July 29, 15:20 is sent as 2111520. |

### If transaction result is negative (01)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 13 | 24 | A | Result description. This field shall report, using only ASCII characters, the reason of the denial. The field is left aligned, filled with blank on the right. |
| 37 | 11 | N | Reserved – fixed to ‘0’ (0x30) |

### Common to any response

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 48 | 1 | N | Card type:  - ‘1’ = Bancomat card - ‘2’ = Credit card - '3' = Other card |
| 49 | 11 | N | Acquirer ID. The field is left aligned, filled with blanks on the right. |
| 60 | 6 | N | STAN. Transaction sequence number. |
| 66 | 6 | N | ID online. Online operation progressive number. |
| 72 | 3 | N | Action code related to current payment operation. |
| 75 | 8 | N | Transaction amount original, right aligned, with ‘0’ filling on the left. Amount is always considered to be expressed in cents (e.g. 650 = 6,50) |
| 83 | 1 | N | Currency exchange transaction flag. The value is set to ‘1’ only when the transaction has been performed with a currency different from terminal currency.  **Important**: if this value is ‘0’, following fields are not meaningful. |
| 84 | 8 | N | Exchange rate, right aligned with 4 decimal digits. – Received by the host "DCC". |
| 92 | 3 | A | Alphanumeric currency code of transaction currency, as received by the host "DCC". |
| 95 | 12 | N | Transaction amount in the transaction currency, right aligned with ‘0’ (0x30) filler on the left - Received by the host “DCC”. |
| 107 | 1 | N | Precision (number of decimals) of transaction currency. |
| 108 | 10 | N | Reserved – fixed to ‘0’ (0x30) |
--------------------------
---
title: "Payment with extended result | Traditional POS | Nexi group developer portal"
source: "https://developer.nexigroup.com/traditionalpos/en-EU/docs/payment-with-extended-result/"
author:
published:
created: 2026-05-27
description:
tags:
  - "clippings"
---
## Payment with extended result

## Extended payment request message (from ECR)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘X’ (0x58) |
| 11 | 8 | N | Cash register ID |
| 19 | 1 | N | Presence of message with additional data for the GT: |
| 20 | 2 | N | Reserved – fixed to ‘0’ (0x30) |
| 22 | 1 | N | Start transaction when card already present:  - ‘0’ = start with card not yet inserted in the terminal - ‘1’ = start with card already inserted in the terminal |
| 23 | 1 | N | Payment type:  - ‘0’ = automatic card recognition on terminal with customer selection in case of cobranded cards - ‘1’ = only debit cards - ‘2’ = only credit cards - ‘3’ = other cards |
| 24 | 8 | N | Transaction amount, right aligned, filled with ‘0’ (0x30) on the left. Amount is always considered to be expressed in cents (e.g. 650 = 6,50) |
| 32 | 128 | AN | Text to be print (Code contract) at the end of the payment receipt. The field is always 128 chars long, it’s right aligned, filled with ‘ ‘ (0x20) on the left. The terminal shall only print valid chars, skipping the blanks. |
| 160 | 8 | N | Reserved – fixed to ‘0’ (0x30) |

## Extended payment response message (from Terminal)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘E’ (0x45) |
| 11 | 2 | N | Transaction result:  - “00” = OK - “01” = KO - “05” = card not present (valid only if field 22 in request message is ‘1’ = start with card already present - “09” = received unknown Tag from GT |

### If transaction result is positive (00)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 13 | 19 | N | Card PAN, right aligned, filled with ‘0’ (0x30) on the left. The PAN is |
| 32 | 3 | A | Transaction type:  - “ICC” = ICC card - “MAG” = magnetic stripe card - “MAN” = manual PAN entry - “CLM” = c-less magstripe - “CLI” = c-less ICC |
| 35 | 6 | A | Authorization code received from host |
| 41 | 7 | A | Transaction date and time received from host in DDDHHMM. The day is expressed as number of days since 01/01 of current year. For example, July 29, 15:20 is sent as 2111520. |

### If transaction result is negative (01)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 13 | 24 | A | Result description. This field shall report, using only ASCII characters, the reason of the denial. The field is left aligned, filled with blank on the right. |
| 37 | 11 | N | Reserved – fixed to ‘0’ (0x30) |

### Common to any response

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 48 | 1 | N | Card type: |
| 49 | 11 | N | Acquirer ID. The field is left aligned, filled with blanks on the right. |
| 60 | 6 | N | STAN. Transaction sequence number. |
| 66 | 6 | N | ID online. Online operation progressive number. |
| 72 | 3 | N | Action code related to current payment operation. |
| 75 | 8 | N | Transaction amount received by host, right aligned, with ‘0’ filling on the left. Amount is always considered to be expressed in cents (e.g. 650 = 6,50) |
| 83 | 10 | N | Reserved – fixed to ‘0’ (0x30) |
--------------------------
---
title: "Traditional POS | Nexi group developer portal"
source: "https://developer.nexigroup.com/traditionalpos/en-EU/docs/reversal/"
author:
published:
created: 2026-05-27
description:
tags:
  - "clippings"
---
## Reversal

## Reversal request message (from ECR)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘S’ (0x53) |
| 11 | 8 | N | Cash register ID |
| 19 | 6 | N | STAN of the transaction to be reversed.  If the field is filled with ‘0’ (0x30) no check is performed.  If the value is different from “000000”, the terminal performs the reversal only if the value is equal to the last payment transaction STAN. |
| 25 | 1 | N | Presence of message with additional data for the GT: |
| 26 | 1 | N | Reserved – fixed to ‘0’ (0x30)  (Terminal requires the same card to perform the reversal) |

## Reversal response message (from Terminal)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘E’ (0x45) |
| 11 | 2 | N | Transaction result:  - “00” = OK - “01” = KO - “09” = received unknown Tag from GT |

### If transaction result is positive (00)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 13 | 19 | N | Card PAN, right aligned, filled with ‘0’ (0x30) on the left. The PAN is |
| 32 | 3 | A | Transaction type:  - “ICC” = ICC card - “MAG” = magnetic stripe card - “MAN” = manual PAN entry - “CLM” = c-less magstripe - “CLI” = c-less ICC |
| 35 | 6 | A | Reserved – fixed to ‘0’ (0x30) |
| 41 | 7 | A | Transaction date and time received from host in DDDHHMM. The day is expressed as number of days since 01/01 of current year. For example, July 29, 15:20 is sent as 2111520. |

### If transaction result is negative (01)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 13 | 24 | A | Result description. This field shall report, using only ASCII characters, the reason of the denial. The field is left aligned, filled with blank on the right. |
| 37 | 11 | N | Reserved – fixed to ‘0’ (0x30) |

### Common to any response

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 48 | 1 | N | Card type: |
| 49 | 11 | N | Acquirer ID. The field is left aligned, filled with blanks on the right. |
| 60 | 6 | N | STAN. Transaction sequence number. |
| 66 | 6 | N | ID online. Online operation progressive number. |
| 72 | 3 | N | Action code related to current payment operation. |
| 75 | 10 | N | Reserved – fixed to ‘0’ (0x30) |
--------------------------
---
title: "Traditional POS | Nexi group developer portal"
source: "https://developer.nexigroup.com/traditionalpos/en-EU/docs/pre-authorization-request/"
author:
published:
created: 2026-05-27
description:
tags:
  - "clippings"
---
## Pre-Authorization request

## Pre-authorization request message (from ECR)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘p’ (0x70) |
| 11 | 8 | N | Cash register ID |
| 19 | 1 | N | Presence of message with additional data for the GT: |
| 20 | 2 | N | Reserved – fixed to ‘0’ (0x30) |
| 22 | 1 | N | Start transaction when card already present:  - ‘0’ = start with card not yet inserted in the terminal - ‘1’ = start with card already inserted in the terminal |
| 23 | 1 | N | Payment type:  - ‘0’ = automatic card recognition on terminal with customer selection in case of cobranded cards - ‘1’ = only debit cards - ‘2’ = only credit cards - ‘3’ = other cards |
| 24 | 8 | N | Pre-authorization transaction amount, right aligned, filled with ‘0’ (0x30) on the left. Amount is always considered to be expressed in cents (e.g. 650 = 6,50) |
| 32 | 128 | AN | Text to be print (Code contract)at the end of the payment receipt. The field is always 128 chars long, it’s right aligned, filled with ‘ ‘ (0x20) on the left. The terminal shall only print valid chars, skipping the blanks. |
| 160 | 8 | N | Reserved – fixed to ‘0’ (0x30) |

## Pre-authorization response message (from Terminal)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘e’ (0x65) |
| 11 | 2 | N | Transaction result:  - “00” = OK - “01” = KO - “05” = card not present (valid only if field 22 in request message is ‘1’ = start with card already present - “09” = received unknown Tag from GT |

### If transaction result is positive (00)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 13 | 19 | N | Card PAN, right aligned, filled with ‘0’ (0x30) on the left. The PAN is |
| 32 | 3 | A | Transaction type:  - “ICC” = ICC card - “MAG” = magnetic stripe card - “MAN” = manual PAN entry - “CLM” = c-less magstripe - “CLI” = c-less ICC |
| 35 | 6 | A | Authorization code received from host |
| 41 | 8 | N | Amount pre-authorized by the Host, right-aligned, with filler 0 (30 hex) on the left. The amount is always understood in euro cents (e.g. 6,50€ = 650). |
| 49 | 9 | N | Unique identifier of the pre-authorization received from Host and printed on the pre-authorization receipt issued by the terminal (Preauthorization Code). |
| 58 | 3 | N | Action Code of transaction received from Host. |
| 61 | 7 | N | The temporal parameters of transaction are received from Host in DDDHHMM format. The day is expressed as a sequential number from Jan 1st = 1 to Dec 31st = 365/366 of the current year. |
| 68 | 3 | N | Reserved – fixed to ‘0’ (0x30) |

### If transaction result is negative (01)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 13 | 24 | A | Result description. This field shall report, using only ASCII characters, the reason of the denial. The field is left aligned, filled with blank on the right. |
| 37 | 3 | N | Action Code of transaction received from Host. |
| 40 | 31 | N | Reserved – fixed to ‘0’ (0x30) |

### Common to any response

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 48 | 1 | N | Card type: |
| 72 | 11 | N | Acquirer ID. The field is left aligned, filled with blanks on the right. |
| 83 | 6 | N | STAN. Transaction sequence number. |
| 89 | 6 | N | ID online. Online operation progressive number. |
| 95 | 12 | N | Reserved – fixed to ‘0’ (0x30) |
--------------------------
---
title: "Traditional POS | Nexi group developer portal"
source: "https://developer.nexigroup.com/traditionalpos/en-EU/docs/incremental-authorization-transaction/"
author:
published:
created: 2026-05-27
description:
tags:
  - "clippings"
---
## Incremental authorization transaction

## Incremental authorization request message (from ECR)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘i’ (0x69) |
| 11 | 8 | N | Cash register ID |
| 19 | 1 | N | Presence of message with additional data for the GT: |
| 20 | 4 | N | Reserved – fixed to ‘0’ (0x30) |
| 24 | 8 | N | Authorization transaction amount, right aligned, filled with ‘0’ (0x30) on the left. Amount is always considered to be expressed in cents (e.g. 650 = 6,50) |
| 32 | 128 | AN | Text to be print (Code contract) at the end of the payment receipt. The field is always 128 chars long, it’s right aligned, filled with ‘ ‘ (0x20) on the left. The terminal shall only print valid chars, skipping the blanks. |
| 160 | 9 | N | Unique identifier of the pre-authorization received from Host and printed on the pre-authorization receipt issued by the terminal (Original Preathorization Code) |
| 169 | 8 | N | Reserved – fixed to ‘0’ (0x30) |

## Incremental authorization response message (from Terminal)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘i’ (0x69) |
| 11 | 2 | N | Transaction result:  - “00” = OK - “01” = KO - “09” = received unknown tag from GT |
| 13 | 19 | N | Card PAN, right aligned, filled with ‘0’ (0x30) on the left. The PAN is sent following PAN truncation rules. |
| 32 | 3 | A | Transaction type: |
| 35 | 11 | N | Acquirer ID. The field is left-aligned with filler Blank on the right. |
| 46 | 6 | A | Authorization code received from host |
| 52 | 6 | N | STAN. Transaction sequence number. |
| 58 | 6 | N | ID online. Online operation progressive number. |
| 64 | 7 | A | Transaction date and time received from host in DDDHHMM. The day is expressed as number of days since 01/01 of current year. For example, July 29, 15:20 is sent as 2111520. |
| 71 | 3 | N | Action Code of transaction received from Host. |
| 74 | 2 | N | Reserved – fixed to ‘0’ (0x30) |
--------------------------
---
title: "Traditional POS | Nexi group developer portal"
source: "https://developer.nexigroup.com/traditionalpos/en-EU/docs/pre-authorization-closure/"
author:
published:
created: 2026-05-27
description:
tags:
  - "clippings"
---
## Pre-authorization closure

## Pre-authorization closure request message (from ECR)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘i’ (0x63) |
| 11 | 8 | N | Cash register ID |
| 19 | 1 | N | Presence of message with additional data for the GT: |
| 20 | 4 | N | Reserved – fixed to ‘0’ (0x30) |
| 24 | 8 | N | Authorization transaction amount, right aligned, filled with ‘0’ (0x30) on the left. Amount is always considered to be expressed in cents (e.g. 650 = 6,50) |
| 32 | 128 | AN | Text to be print (Code contract) at the end of the payment receipt. The field is always 128 chars long, it’s right aligned, filled with ‘ ‘ (0x20) on the left. The terminal shall only print valid chars, skipping the blanks. |
| 160 | 9 | N | Unique identifier of the pre-authorization received from Host and printed on the pre-authorization receipt issued by the terminal (Original Preathorization Code) |
| 169 | 8 | N | Reserved – fixed to ‘0’ (0x30) |

## Pre-authorization closure response message without currency exchange data (from Terminal)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘c’ (0x63) |
| 11 | 2 | N | Transaction result:  - “00” = OK - “01” = KO - “09” = received unknown Tag from GT |

### If transaction result is positive (00)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 13 | 19 | N | Card PAN, right aligned, filled with ‘0’ (0x30) on the left. The PAN is |
| 32 | 3 | A | Transaction type:  - “ICC” = ICC card - “MAG” = magnetic stripe card - “MAN” = manual PAN entry - “CLM” = c-less magstripe - “CLI” = c-less ICC |
| 35 | 6 | A | Authorization code received from host |
| 41 | 7 | A | Transaction date and time received from host in DDDHHMM. The day is expressed as number of days since 01/01 of current year. For example, July 29, 15:20 is sent as 2111520. |

### If transaction result is negative (01)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 13 | 24 | A | Result description. This field shall report, using only ASCII characters, the reason of the denial. The field is left aligned, filled with blank on the right. |
| 37 | 11 | N | Reserved – fixed to ‘0’ (0x30) |

### Common to any response

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 48 | 1 | N | Card type:  - '1' = Bancomat card - '2' = Credit card - '3' = Other card |
| 49 | 11 | N | Acquirer ID. The field is left aligned, filled with blanks on the right. |
| 60 | 6 | N | STAN. Transaction sequence number. |
| 66 | 6 | N | ID online. Online operation progressive number. |
| 72 | 3 | N | Action Code relating to the current payment operation. The field, if present, applies either to the positive and negative transactions. |

## Pre-authorization closure response message with currency exchange data (from Terminal)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘v’ (0x76) |
| 11 | 2 | N | Transaction result:  - “00” = OK - “01” = KO - “09” = received unknown Tag from GT |

### If transaction result is positive (00)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 13 | 19 | N | Card PAN, right aligned, filled with ‘0’ (0x30) on the left. The PAN is |
| 32 | 3 | A | Transaction type:  - “ICC” = ICC card - “MAG” = magnetic stripe card - “MAN” = manual PAN entry - “CLM” = c-less magstripe - “CLI” = c-less ICC |
| 35 | 6 | A | Authorization code received from host |
| 41 | 7 | A | Transaction date and time received from host in DDDHHMM. The day is expressed as number of days since 01/01 of current year. For example, July 29, 15:20 is sent as 2111520. |

### If transaction result is negative (01)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 13 | 24 | A | Result description. This field shall report, using only ASCII characters, the reason of the denial. The field is left aligned, filled with blank on the right. |
| 37 | 11 | N | Reserved – fixed to ‘0’ (0x30) |

### Common to any response

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 48 | 1 | N | Card type:  - '1' = Bancomat card - '2' = Credit card - '3' = Other card |
| 49 | 11 | N | Acquirer ID. The field is left aligned, filled with blanks on the right. |
| 60 | 6 | N | STAN. Transaction sequence number. |
| 66 | 6 | N | ID online. Online operation progressive number. |
| 72 | 3 | N | Action Code relating to the current payment operation. The field, if present, applies either to the positive and negative transactions. |
| 75 | 1 | N | Currency exchange transaction flag. The value is set to ‘1’ only when the transaction has been performed with a currency different from terminal currency.  **Important**: if this value is ‘0’, following fields are not meaningful. |
| 76 | 8 | N | Exchange rate, right aligned with 4 decimal digits. – Received by the host “DCC”. |
| 84 | 3 | A | Alphanumeric currency code of transaction currency, as received by the host. |
| 87 | 12 | N | Transaction amount in the transaction currency, right aligned with ‘0’ (0x30) filler on the left - Received by the host “DCC”. |
| 99 | 1 | N | Precision (number of decimals) of transaction currency. |
| 100 | 10 | N | Reserved – fixed to ‘0’ (0x30) |
--------------------------
---
title: "Traditional POS | Nexi group developer portal"
source: "https://developer.nexigroup.com/traditionalpos/en-EU/docs/card-verification/"
author:
published:
created: 2026-05-27
description:
tags:
  - "clippings"
---
## Card verification

## Card Verification request message (from ECR)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘H’ (0x48) |
| 11 | 8 | N | Cash register ID |
| 19 | 1 | N | Presence of message with additional data for the GT: |
| 20 | 2 | N | Reserved – fixed to ‘0’ (0x30) |
| 22 | 1 | N | ‘0’ = standard card verification |
| 23 | 1 | N | Payment type:  - ‘0’ = automatic card recognition on terminal with customer selection in case of cobranded cards - ‘1’ = only debit cards - ‘2’ = only credit cards - ‘3’ = other cards |
| 24 | 16 | N | Reserved – fixed to ‘0’ (0x30) |

## Card Verification response message (from Terminal)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘E’ (0x45) |
| 11 | 2 | N | Transaction result:  - “00” = OK - “01” = KO - “09” = received unknown Tag from GT |

### If transaction result is positive (00)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 13 | 19 | N | Card PAN, right aligned, filled with ‘0’ (0x30) on the left. The PAN is |
| 32 | 3 | A | Transaction type:  - “ICC” = ICC card - “MAG” = magnetic stripe card - “MAN” = manual PAN entry - “CLM” = c-less magstripe - “CLI” = c-less ICC |
| 35 | 6 | A | Authorization code received from host |
| 41 | 7 | A | Transaction date and time received from host in DDDHHMM. The day is expressed as number of days since 01/01 of current year. For example, July 29, 15:20 is sent as 2111520. |

### If transaction result is negative (01)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 13 | 24 | A | Result description. This field shall report, using only ASCII characters, the reason of the denial. The field is left aligned, filled with blank on the right. |
| 37 | 3 | N | Action code related to current payment operation. |
| 40 | 8 | N | Reserved – fixed to ‘0’ (0x30) |

### Common to any response

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 48 | 1 | N | Card type: |
| 49 | 11 | N | Acquirer ID. The field is left aligned, filled with blanks on the right. |
| 60 | 6 | N | STAN. Transaction sequence number. |
| 66 | 6 | N | ID online. Online operation progressive number. |
--------------------------
---
title: "Additional data to or from host | Traditional POS | Nexi group developer portal"
source: "https://developer.nexigroup.com/traditionalpos/en-EU/docs/additional-data-to-or-from-host/"
author:
published:
created: 2026-05-27
description:
tags:
  - "clippings"
---
## Additional data to or from host

## Overview

Additional tags are used to send data in the authorization request that are returned in the settlement logs to the Merchant or are used by NEXI to activate specific services, like tokenization.

The formatting of these tags is different per Bank:

**Intesa San Paolo**

- Data can be inserted in the tag with index 5.
- The maximum length of the tag is 100 bytes.

**Other Banks**

- Data can be inserted in a maximum of 4 tags with index from 1 to 5.
- The maximum length of concatenated data is 100 bytes (not considering control characters and separators).
- The maximum length of tags from 1 to 4 is 30 bytes
- The maximum length of tag 5 is 100 bytes

The ADDITIONAL DATA FOR GT application message is only expected from the POS terminal when, in the single procedure activation commands, the relevant flag is found (“Presence of message with additional data for the GT”).

There are two fields in this message that inform the terminal whether it should send the “additional data from GT message” after the procedure result message. The message with additional data is the only message that contains fields with variable length, indicated with the letter **V** in the **LENGTH** column in the table below.

## Additional data for GT delivery message (from ECR)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘U’ (0x55) |
| 11 | 6 | N | Payment Type:  - “0” => Standard payment |
| 17 | 2 | N | ISO field number.  Indicates the ISO-8583 field where data from GT that must be sent to ECR is found. This field is “00” by default and indicates that there is not data from GT to return to ECR. A value other than “00” alerts the terminal of:  - data from GT retrieval coordinates - obligation to send additional data from GT message  **CURRENTLY THE FIELD IS SET AT “62”** |
| 19 | 8 | A | TAG number.  Indicates the TAG where data from GT that must be sent to ECR is found. This field is only significant if the previous field is not “00”. The field is flush left, blank justified. TAG from GT length is fixed at 255.  **CURRENTLY THE FIELD IS FIXED AT ‘DF8D01’** |
| 27 | 1 | N | Reserved (fixed at “0” 30 hex) |
| 28 | 4 | N | Exclusive TAG index with additional data to be sent to GT.  The field is considered as a byte map for which each byte corresponds to a single index. “0” corresponds to no TAG to be sent to GT. |
| 32 | 5 | N | Reserved (fixed at “0” 30 hex) |
| 37 | 100-V | A | Privative TAG content. Max 100 characters - Min 1 character. The field is always closed by an end-of-field character 01B hex |

The “Exclusive TAG content” field can be repeated for a maximum of 4 times.

## Additional data from GT result message (from Terminal)

The additional data from GT message is only sent by the POS terminal if the ISO number field in the "Message with additional data for GT" is not "00".

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘U’ (0x55) |
| 11 | 6 | N | Reserved (fixed at “0” 30 hex) |
| 17 | 3 | N | Data from GT field length. Indicates the length of the next field |
| 20 | 255-V | A | Additional data from GT that must be sent to ECR |
| 20+n | 10 | N | Reserved (fixed at “0” 30 hex) |

## Tokenization Request

The transaction requested to activate tokenization via LAN Integration must include Additional Data, in the format requested for the service, with the following information:

- Type of Tokenization service requested (Recurring, Unscheduled, One Click)
- Contract code: the code has to be unique at Merchant level and is alphanumeric 18 characters long.

### Additional TAGs - Intesa San Paolo

| Length | Type | Content |
| --- | --- | --- |
| 4 | An | Identification code of the service:  - “0COF” – Unscheduled Recurring and One Click Payments - “0REC” – Recurring Payments |
| 4 | An | Fixed value “0TRK” |
| Ans..18 | An | Unique contract code |
| 1 | Ans 1 | “\|” (pipe) |
| 4 | An | Service code “Labelling Omnichannel”  Fixed code “0FNZ03” |

Example TAG 5 mapping for Unscheduled Recurring and One Click Payments with contract code 1666354841608 Es: 0COF0TRK1666354841608|0FNZ03

### TAGs – Other Banks

| Index (TAG) | Position (Byte) | Length | Type | Content |
| --- | --- | --- | --- | --- |
| 1 | 1 | 3 | A/N | Fixed value “BTD” |
|  | 4 | Variabile (max 18 bytes) | A/N | Unique contract code |
| 5 | 1 | 4 | A/N | Identification code of the service:  - “0COF” – Unscheduled Recurring and One Click Payments - “0REC” – Recurring Payments |
|  | 5 | 4 | A/N | Fisso a “0FNZ” |
|  | 9 | 2 | A/N | Action:  “03” – new contract creation |

Example TAGs mapping for Unscheduled Recurring and One Click Payments with contract code *9297022f-75de-48*.

| Index (TAG) | Position (Byte) | Length | Type | Content |
| --- | --- | --- | --- | --- |
| 1 | 1 | 3 | A/N | BTD |
|  | 4 | Variable | A/N | 9297022f-75de-48 |
| 5 | 1 | 4 | A/N | 0COF |
| 5 | 5 | 4 | A/N | 0FNZ |
| 5 | 9 | 2 | A/N | 03 |
--------------------------
---
title: "Traditional POS | Nexi group developer portal"
source: "https://developer.nexigroup.com/traditionalpos/en-EU/docs/close-session/"
author:
published:
created: 2026-05-27
description:
tags:
  - "clippings"
---
## Close session

## Close Session request message (from ECR)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘C’ (0x43) |
| 11 | 8 | N | Cash register ID |
| 19 | 1 | N | Presence of message with additional data for the GT: |
| 20 | 7 | N | Reserved – fixed to ‘0’ (0x30) |

## Close Session response message (from Terminal)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘C’ (0x43) |
| 11 | 2 | N | Transaction result:  - “00” = OK - “01” = KO - “09” = received unknown tag from GT |

### If transaction result is positive (00)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 13 | 16 | N | EFT-POS Total amount, right aligned, filled with ‘0’ (0x30) on the left. Amount is always considered to be expressed in cents (e.g. 650 = 6,50) |
| 29 | 16 | N | Host Total amount, right aligned, filled with ‘0’ (0x30) on the left. Amount is always considered to be expressed in cents (e.g. 650 = 6,50) |

### If transaction result is negative (01)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 13 | 19 | A | Result description. This field shall report, using only ASCII characters,  the reason of the denial. The field is left aligned, filled with blank on the right. |
| 32 | 3 | N | Action code related to current payment operation |
| 35 | 10 | N | Reserved – fixed to ‘0’ (0x30) |
--------------------------
---
title: "Traditional POS | Nexi group developer portal"
source: "https://developer.nexigroup.com/traditionalpos/en-EU/docs/terminal-totals/"
author:
published:
created: 2026-05-27
description:
tags:
  - "clippings"
---
## Terminal totals

## Terminal Totals request message (from ECR)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘T’ (0x54) |
| 11 | 8 | N | Cash register ID |
| 19 | 1 | N | Presence of message with additional data for the GT: |
| 20 | 7 | N | Reserved – fixed to ‘0’ (0x30) |

## Terminal Totals response message (from Terminal)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘T’ (0x54) |
| 11 | 2 | N | Transaction result: |
| 13 | 16 | N | EFT-POS Total amount, right aligned, filled with ‘0’ (0x30) on the left. Amount is always considered to be expressed in cents (e.g. 650 = 6,50) |
| 29 | 6 | A | Reserved – fixed to ‘0’ (0x30) |
--------------------------
---
title: "Traditional POS | Nexi group developer portal"
source: "https://developer.nexigroup.com/traditionalpos/en-EU/docs/send-last-result/"
author:
published:
created: 2026-05-27
description:
tags:
  - "clippings"
---
## Send last result

## Overview

Whenever the POS terminal runs one of the following transactions, commanded by the remote device: Payment (all types), Offset, Credit, Pre-authorization (request, integration, and closure) it saves the transaction result message sent by the remote device in its static memory.

If saved, it includes the “additional data from GT result” message. When the remote device wants to retrieve these results, it sends the terminal this command.

The terminal does not run any sequence check and always keeps this message even if any other transaction was run that does not require the result message to be saved (example, DLL, totals, etc.).

## Receipt reprint request message (from ECR)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘G’ (0x47) |
| 11 | 8 | N | Cash register ID |
| 19 | 1 | N | Presence of message with additional data for the GT: |
| 20 | 3 | N | Reserved (fixed at “0” 30 hex) |

## Last result request feedback message (from ECR)

The feedback message is exactly the same as the last RESULT message saved during the foreseen procedures. If required by the command message, the additional data from GT message is also sent.

The sequence is the same as a normal command result.
--------------------------
---
title: "Enable and disable printing receipt on ECR | Traditional POS | Nexi group developer portal"
source: "https://developer.nexigroup.com/traditionalpos/en-EU/docs/enable-and-disable-printing-receipt-on-ecr/"
author:
published:
created: 2026-05-27
description:
tags:
  - "clippings"
---
## Enable and disable printing receipt on ECR

## Overview

The application message **"Enable/disable printing receipt on ECR"** can only have the confirmation message (ACK-ETX-LRC) as its reply.

When the POS terminal goes back to idle, it automatically enables receipts printing on its printer. At the beginning of each procedure triggered by the ECR, the **“Enable printing receipt on ECR”** command is be sent if that is required.

- If the terminal does not receive any **“Enable/disable printing receipt on ECR”** message, or if it receives the command of **"Disable printing receipt on ECR"**, the receipt of the requested procedure is printed by the terminal.
- If the terminal receives the **"Enable printing receipt on ECR"** command, the receipt of the requested procedure is always sent to the ECR. The POS terminal sends the receipt lines by using the **"S"** command.

At the end of any procedure requested by the cash register, the terminal automatically returns the **"Print on terminal enabled"** status.

## Enable and disable printing receipt on ECR message (from ECR)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | A | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘E’ (0x45) |
| 11 | 1 | N | Enable disable printing receipt on ECR flag: - **‘0’** - Disable printing receipt on ECR (any receipt will be printed by the terminal). - **‘1’** - Enable printing receipt on ECR (any receipt will be sent to the ECR by using the "S" command). |
--------------------------
---
title: "Traditional POS | Nexi group developer portal"
source: "https://developer.nexigroup.com/traditionalpos/en-EU/docs/send-ticket/"
author:
published:
created: 2026-05-27
description:
tags:
  - "clippings"
---
## Send ticket

## Overview

Through this command, the Terminal sends the lines of the receipt to ECR for printing. Every message can contain up to a maximum of 200 characters. Multiple lines can be sent for printing, with different formats indicated by specific control characters.

- **Standard format or double height format**: both normal and bold are available, lines are composed of 24 characters.
- **Double width format:** both normal and bold are available, lines are composed of 12 characters.
- **Compressed format:** lines are composed of 42 characters.

- The new line character 0x7D resets the format to standard - 24 characters. If it’s the first character of a new line, it should be interpreted as a request to print an “empty line” (24 black).
- If the line is 24 characters long, the 0x7D is not required because the printer automatically considers the line as “completed”, so the next character will be printed in a new line.
- In case the line of printing is preceded by the characters 0x7B, 0x7C or 0x5E, the number of characters after which a line wrap is inserted is always 12 or 42. Therefore, it is not necessary to insert an end-of-line character 0x7D.
- If a row should be printed in “double height bold” (as an example: a row with the amount when the transaction is successful), the first character of this row must be 0x7F. If the line is 24 characters long, the next one will be printed automatically in the normal format, without the presence of any special character.

## Send ticket message (from Terminal)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | N | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘S’ (0x53) |
| 11 | 1-200 | A | Lines to be printed; formatted to 12, 14 or 42 chars per line. |

**Field 11** can contain the following special characters, with the specified meaning:

- 0x7D = new line (CR+LF) and normal character format reset - USED
- 0x7E = start of double height normal characters - NOT USED
- 0x7F = start of double height bold characters - USED
- 0x7B = start of double width normal characters - NOT USED
- 0x7C = start of double width bold characters - NOT USED
- 0x5E = start of compressed characters - NOT USED

The last **Send** ticket message will at the end of **Field 11** contain the following sequence:

- 0x7D = new line (CR+LF)
- 0x7D = new line (CR+LF)
- 0x7D = new line (CR+LF)
- 0x7D = new line (CR+LF)
- 0x7D = new line (CR+LF)
- 0x7D = new line (CR+LF)
- 0x1B = ticket end
--------------------------
---
title: "Traditional POS | Nexi group developer portal"
source: "https://developer.nexigroup.com/traditionalpos/en-EU/docs/reprint-ticket/"
author:
published:
created: 2026-05-27
description:
tags:
  - "clippings"
---
## Reprint ticket

## Overview

This command can be used to ask the "POS" terminal to print a copy of the last financial receipt created. The command contains the indication of whether the copy of the receipt must be printed directly from the terminal on its own printer or if the copy must be sent to the ECR.

In case the copy must be sent to the ECR, after having confirmed the "reprint ticket” command, the POS terminal sends the copy of the receipt to the ECR by using the "S" type commands.

If the "POS" terminal is without a printer, the “reprint ticket” command with direct printing on the terminal does not produce any results.

## Send ticket message (from Terminal)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | N | Reserved – fixed to ‘0’ (0x30) |
| 10 | 1 | A | Message code: ‘R’ (0x52) |
| 11 | 1 | N | Enable disable printing receipt on ECR flag:  - **‘0’** = disable printing receipt on ECR (any receipt will be print by terminal) - **‘1’** = enable printing receipt on ECR (any receipt will be sent to the ECR using ‘S’ command) |
| 12 | 1 | N | Ticket type flag: |
| 13 | 10 | A | Reserved – fixed to ‘0’ (0x30) |
--------------------------
---
title: "VAS request - K command | Traditional POS | Nexi group developer portal"
source: "https://developer.nexigroup.com/traditionalpos/en-EU/docs/vas-request-k-command/"
author:
published:
created: 2026-05-27
description:
tags:
  - "clippings"
---
## VAS request - K command

## Overview

This command can be used to manage the VAS Services (APPs developed by Nexi) for alternative payment methods (APM) and other services. The protocol is a carrier for requests and responses from ECR to the corresponding APPs.

This feature is only available for Traditional POS.

## VAS Request (from ECR)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | N | Reserved (fixed at “0” 30 hex) |
| 10 | 1 | A | Message code: “K” (75 hex) |
| 11 | 8 | N | ECR Identifier |
| 19 | 3 | A | Reserved (fixed at “0” 30 hex) |
| 22 | 1 | N | Reserved (fixed at “0”) |
| 23 | 4 | N | Vas Request Length |
| 27 | max 1024 | A | Vas Request (XML Format). The length of message is specified in “VAS Request Length". |

## VAS Response (from Terminal)

| Pos | Length | Type | Content |
| --- | --- | --- | --- |
| 1 | 8 | N | Terminal ID (00000000-99999999) |
| 9 | 1 | N | Reserved (fixed at “0” 30 hex) |
| 10 | 1 | A | Message code: “K” (75 hex) |
| 11 | 4 | A | Reserved (fixed at “0” 30 hex) |
| 15 | 1 | N | Concatenation Flag:  - **“0”** - last message - **“1”** - more messages to be sent |
| 16 | 3 | N | ID message: identify the message in sequence.  The first message is set to “001”. |
| 23 | 4 | N | Vas Response Length |
| 27 | max 1024 | A | Vas Response (XML Format) |

If the XML response is greater than 1024 bytes (in the form of a long Receipt Data), the response will be formulated in more concatenated messages so that it would be max 1024 bytes.

If the “concatenation Flag” is set to “1”, the ECR will queue more messages (after sending ACK).

The messages are characterized by the “ID Message” field, which is incremental from 001 to the number of messages that compose the response.

ECR will concatenate the messages and compose the final XML “VAS Response”.

The response XML can contain the receipt produced in response to the execution of the requested VAS. The receipt, also in XML format, can contain the following TAGs for print formatting:

<table><thead><tr><th>TAGs</th><th>Meaning</th></tr></thead><tbody><tr><td><code><t>…</t></code></td><td>The text to be printed out</td></tr><tr><td><code><lf /></code></td><td>Line feed</td></tr><tr><td><code><b /></code></td><td>Start printing in bold</td></tr><tr><td><code><dh /></code></td><td>Start printing in double height</td></tr><tr><td><code><dw /></code></td><td>Start printing in double weight</td></tr><tr><td><code><n /></code></td><td>Start printing in normal</td></tr><tr><td><code><nb /></code></td><td>Switch from bold to normal</td></tr><tr><td><code><nh /></code></td><td>Switch to normal height</td></tr><tr><td><code><nw /></code></td><td>Switch to normal weight</td></tr><tr><td colspan="100"></td></tr></tbody></table>

## APM (BancomatPAY, ALIPAY, WECHAT)

APM management is based on the use of the VAS\_CLIENT application. Services can be engaged by ECR using the “K” command.

The management of APMs involves a single delivery method. These are the supported functions:

| Function | Description |
| --- | --- |
| Payment via QrCode | Triggers the flow for managing payment via QR-CODE on the terminal. The transaction amount is passed as an optional parameter. |
| Last operation status | Triggers the flow on the terminal to verify the outcome of the last operation performed. |
| Total or Partial Refund | Triggers the flow for managing the reversal of a payment on the terminal. The cancellation can be total or partial. |
| Daily Totals | Triggers the request for totals (number of transactions and amount) from the last accounting close. |
| Accounting Closure | Triggers the request for accounting closure with zeroing of the totals. |

### Payment via QrCode - Request

### Payment via QrCode - Request

```json
<ecrreq>
<p k="ECRVASID">BPAY_QR</p>
<p k="SUBSMPARAMS">
<p k="AMOUNT">value</p>
```

| Name | Description | Type | Values |
| --- | --- | --- | --- |
| ECRVASID | Request Service ID | Alphanumeric | BPAY\_QR  WECHAT\_QR  ALIPAY\_QR |
| AMOUNT | Amount to authorize | Numeric |  |
| PRINTER | Defines who prints the receipt. If omitted, print the POS. | Alphanumeric | POS  ECR |

### Payment via QrCode - Request

```json
"<ecrreq> <p k=\"ECRVASID\">BPAY_QR</p><p k=\"SUBSMPARAMS\"><p k=\"AMOUNT\">10</p><p k=\"PRINTER\">ECR</p></p></ecrreq>"
```

### Payment via QrCode - Response

### Payment via QrCode - Response

```json
<ecrres>
<p k="ECRVASID">BPAY_QR</p>
<p k="RESPID">0</p>
<p k="RESPMSG">OK-APPROVED</p>
```

| Name | Description | Type | Values |
| --- | --- | --- | --- |
| ECRVASID | Request Service ID | Alphanumeric | BPAY\_QR  WECHAT\_QR  ALIPAY\_QR |
| RESPID | Outcome of the Request | Numeric | 0 = OK  0 <> KO |
| RESPMSG | Description Outcome | Alphanumeric | If OK –-> APPROVED  If not OK –-> Description of the anomaly found |
| ORDER\_ID | Unique identifier attributed to the transaction | Alphanumeric, max 27 | Value returned by the circuit. Necessary for the management of any Refund request. |

### Last operation status - Request

### Last operation status - Request

```json
<ecrreq>
<p k="ECRVASID">BPAY_ INQUIRY</p>
<p k="SUBSMPARAMS">
<p k="PRINTER">value</p>
```

| Name | Description | Type | Values |
| --- | --- | --- | --- |
| ECRVASID | Request Service ID | Alphanumeric | BPAY\_INQUIRY  WECHAT\_INQUIRY  ALIPAY\_INQUIRY |
| PRINTER | Defines who prints the receipt. If omitted, print the POS. | Alphanumeric | POS  ECR |

### Last operation status - Request

```json
"<ecrreq> <p k=\"ECRVASID\">BPAY_QR</p><p k=\"SUBSMPARAMS\"><p k=\"PRINTER\">ECR</p></p></ecrreq>"
```

### Last operation status - Response

### Last operation status - Response

```json
<ecrres>
<p k="ECRVASID">BPAY_ INQUIRY</p>
<p k="RESPID">0</p>
<p k="RESPMSG"> OK-APPROVED</p>
```

| Name | Description | Type | Values |
| --- | --- | --- | --- |
| ECRVASID | Request Service ID | Alphanumeric | BPAY\_INQUIRY  WECHAT\_INQUIRY  ALIPAY\_INQUIRY |
| RESPID | Outcome of the Request | Numeric | 0 = OK  0 <> KO |
| RESPMSG | Description Outcome | Alphanumeric | If OK –-> APPROVED  If not OK –-> Description of the anomaly found |
| ORDER\_ID | Unique identifier attributed to the transaction | Alphanumeric, max 27 | Value returned by the circuit. Necessary for the management of any Refund request. |

### Total or Partial Refund - Request

### Total or Partial Refund - Request

```json
<ecrreq>
<p k="ECRVASID">BPAY_REFUND</p>
<p k="SUBSMPARAMS">
<p k="AMOUNT">value</p>
```

| Name | Description | Type | Values |
| --- | --- | --- | --- |
| ECRVASID | Request Service ID | Alphanumeric | BPAY\_REFUND  WECHAT\_REFUND  ALIPAY\_ REFUND |
| AMOUNT | Amount to authorize | Numeric | Amount to be reversed (less than or equal to the authorized amount) |
| ORDER\_ID | Unique identifier of the transaction to be refunded | Alphanumeric, max 27 | Assigned by the authorization system |
| PRINTER | Defines who prints the receipt. If omitted, print the POS. | Alphanumeric | POS  ECR |

### Total or Partial Refund - Request

```json
"<ecrreq> <p k=\"ECRVASID\">BPAY_REFUND</p><p k=\"SUBSMPARAMS\"><p k=\"AMOUNT\">10</p><p k=\"PRINTER\">ECR</p></p></ecrreq>"
```

### Total or Partial Refund - Response

### Total or Partial Refund - Response

```json
<ecrres>
<p k="ECRVASID">BPAY_REFUND</p>
<p k="RESPID">0</p>
<p k="RESPMSG"> OK-APPROVED</p>
```

| Name | Description | Type | Values |
| --- | --- | --- | --- |
| ECRVASID | Request Service ID | Alphanumeric | BPAY\_REFUND  WECHAT\_REFUND  ALIPAY\_ REFUND |
| RESPID | Outcome of the Request | Numeric | 0 = OK  0 <> KO |
| RESPMSG | Description Outcome | Alphanumeric | If OK –-> APPROVED  If not OK –-> Description of the anomaly found |
| ORDER\_ID | Unique identifier attributed to the transaction | Alphanumeric, max 27 | Value echo |

### Daily Totals - Request

Currently available only for BPAY.

### Daily Totals - Request

```json
<ecrreq>
<p k="ECRVASID">BPAY_TOTAL</p>
<p k="SUBSMPARAMS">
<p k="PRINTER">value</p>
```

| Name | Description | Type | Values |
| --- | --- | --- | --- |
| ECRVASID | Request Service ID | Alphanumeric | BPAY\_TOTAL |
| PRINTER | Defines who prints the receipt. If omitted, print the POS. | Alphanumeric | POS  ECR |

### Daily Totals - Request

```json
"<ecrreq> <p k=\"ECRVASID\">BPAY_TOTAL</p></ecrreq>"
```

### Daily Totals - Response

### Daily Totals - Response

```json
<ecrres>
<p k="ECRVASID">BPAY_TOTAL</p>
<p k="RESPID">0</p>
<p k="RESPMSG"> OK-APPROVED</p>
```

| Name | Description | Type | Value |
| --- | --- | --- | --- |
| ECRVASID | Request Service ID | Alphanumeric | BPAY\_REFUND |
| RESPID | Outcome of the Request | Numeric | 0 = OK  0 <> KO |
| RESPMSG | Description Outcome | Alphanumeric | If OK –-> APPROVED  If not OK –-> Description of the anomaly found |
| NUM\_TR | Total number of transactions accounted for | Alphanumeric | Value calculated from the last closing |
| TOT\_TR | Total amount accounted for | Alphanumeric | Value calculated from the last closing |

### Accounting Closure - Request

Currently available only for BPAY.

### Accounting Closure - Request

```json
<ecrreq>
<p k="ECRVASID">BPAY_CHIUSURA</p>
<p k="SUBSMPARAMS">
<p k="PRINTER">value</p>
```

| Name | Description | Type | Value |
| --- | --- | --- | --- |
| ECRVASID | Request Service ID | Alphanumeric | BPAY\_CHIUSURA |
| PRINTER | Defines who prints the receipt. If omitted, print the POS. | Alphanumeric | POS  ECR |

### Accounting Closure - Request

```json
"<ecrreq> <p k=\"ECRVASID\">BPAY_CHIUSURA</p></ecrreq>"
```

### Accounting Closure - Response

### Accounting Closure - Response

```json
<ecrres>
<p k="ECRVASID"> BPAY_CHIUSURA</p>
<p k="RESPID">0</p>
<p k="RESPMSG"> OK-APPROVED</p>
```

| Name | Description | Type | Value |
| --- | --- | --- | --- |
| ECRVASID | Request Service ID | Alphanumeric | BPAY\_CHIUSURA |
| RESPID | Outcome of the Request | Numeric | 0 = OK  0 <> KO |
| RESPMSG | Description Outcome | Alphanumeric | If OK –-> APPROVED  If not OK –-> Description of the anomaly found |
--------------------------
