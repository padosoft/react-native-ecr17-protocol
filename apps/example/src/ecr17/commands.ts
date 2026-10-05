// Static metadata describing every ECR17 command and its parameters. Drives the
// command palette and the dynamic parameter form. Pure data — no React, no client.

export type FieldKind = 'money' | 'text' | 'number' | 'bool' | 'enum';

export interface Field {
  name: string;
  label: string;
  kind: FieldKind;
  required?: boolean;
  options?: { label: string; value: string }[];
  placeholder?: string;
}

export interface CommandDef {
  key: string;
  label: string;
  /** ECR17 protocol letter, shown as a chip. */
  letter: string;
  /** Financial / state-changing command — styled distinctly. */
  danger?: boolean;
  fields: Field[];
}

const paymentType: Field = {
  name: 'paymentType',
  label: 'Card type',
  kind: 'enum',
  options: [
    { label: 'auto', value: 'auto' },
    { label: 'debit', value: 'debit' },
    { label: 'credit', value: 'credit' },
    { label: 'other', value: 'other' },
  ],
};

const amount: Field = { name: 'amountCents', label: 'Amount (€)', kind: 'money', required: true };
const cardPresent: Field = { name: 'cardAlreadyPresent', label: 'Card already present', kind: 'bool' };
const receiptText: Field = { name: 'receiptText', label: 'Receipt text', kind: 'text' };

const paymentFields: Field[] = [amount, paymentType, cardPresent, receiptText];

export const COMMANDS: CommandDef[] = [
  { key: 'status', label: 'Status', letter: 's', fields: [] },
  { key: 'pay', label: 'Pay', letter: 'P', danger: true, fields: paymentFields },
  { key: 'payExtended', label: 'Pay (extended)', letter: 'X', danger: true, fields: paymentFields },
  {
    key: 'reverse',
    label: 'Reverse',
    letter: 'S',
    danger: true,
    fields: [{ name: 'stan', label: 'STAN (blank = last)', kind: 'text' }],
  },
  { key: 'preAuth', label: 'Pre-auth', letter: 'p', danger: true, fields: paymentFields },
  {
    key: 'incrementalAuth',
    label: 'Incremental auth',
    letter: 'i',
    danger: true,
    fields: [
      amount,
      { name: 'originalPreAuthCode', label: 'Original pre-auth code', kind: 'text', required: true },
      receiptText,
    ],
  },
  {
    key: 'preAuthClosure',
    label: 'Pre-auth closure',
    letter: 'c',
    danger: true,
    fields: [
      amount,
      { name: 'originalPreAuthCode', label: 'Original pre-auth code', kind: 'text', required: true },
      receiptText,
    ],
  },
  { key: 'verifyCard', label: 'Verify card', letter: 'H', fields: [paymentType] },
  { key: 'closeSession', label: 'Close session', letter: 'C', danger: true, fields: [] },
  { key: 'totals', label: 'Totals', letter: 'T', fields: [] },
  { key: 'sendLastResult', label: 'Send last result (G)', letter: 'G', fields: [] },
  {
    key: 'enableEcrPrinting',
    label: 'ECR printing',
    letter: 'E',
    fields: [{ name: 'enabled', label: 'Enabled', kind: 'bool' }],
  },
  {
    key: 'reprint',
    label: 'Reprint',
    letter: 'R',
    fields: [{ name: 'toEcr', label: 'To ECR', kind: 'bool' }],
  },
  {
    key: 'vas',
    label: 'VAS',
    letter: 'K',
    fields: [{ name: 'xmlRequest', label: 'XML request', kind: 'text', required: true }],
  },
];
