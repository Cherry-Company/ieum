/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

namespace deskflow::licensing::test_fixture {

struct InvalidFixture
{
  const char *name;
  const char *license;
};

inline constexpr InvalidFixture kUnsupportedFixtures[] = {
    {
        "wrong schema",
        "IEUM1.eyJleHBpcmVzX2F0IjpudWxsLCJmZWF0dXJlcyI6WyJmaWxlLXRyYW5zZmVyIl0sImlzc3VlZF9hdCI6IjIwMjYt"
        "MDgtMDFUMDA6MDA6MDBaIiwia2V5X2lkIjoidGVzdC1rZXktMjAyNi0wMSIsImxpY2Vuc2VfaWQiOiIxMTExMTExMS0yMjIy"
        "LTQzMzMtODQ0NC01NTU1NTU1NTU1NTUiLCJub3RfYmVmb3JlIjoiMjAyNi0wOC0wMVQwMDowMDowMFoiLCJwcm9kdWN0Ijoi"
        "cHJvLWxvY2FsIiwicmVjaXBpZW50IjoiZml4dHVyZS1vd25lciIsInNjaGVtYSI6ImlldW0ucHJvLWxpY2Vuc2UudjIifQ."
        "aRcWdz55kBsrg2__MZH-X63Ozk2NcwNLsmvg5JSj23BA86wC1cw-HyV0W1d56ZcrZmh8n4742Z1vdMIIZX9mAA",
    },
    {
        "unknown key",
        "IEUM1.eyJleHBpcmVzX2F0IjpudWxsLCJmZWF0dXJlcyI6WyJmaWxlLXRyYW5zZmVyIl0sImlzc3VlZF9hdCI6IjIwMjYt"
        "MDgtMDFUMDA6MDA6MDBaIiwia2V5X2lkIjoidW5rbm93bi1rZXkiLCJsaWNlbnNlX2lkIjoiMTExMTExMTEtMjIyMi00MzMz"
        "LTg0NDQtNTU1NTU1NTU1NTU1Iiwibm90X2JlZm9yZSI6IjIwMjYtMDgtMDFUMDA6MDA6MDBaIiwicHJvZHVjdCI6InByby1s"
        "b2NhbCIsInJlY2lwaWVudCI6ImZpeHR1cmUtb3duZXIiLCJzY2hlbWEiOiJpZXVtLnByby1saWNlbnNlLnYxIn0."
        "axng1THIyZccsSQ-GyTvree5MuC7rYvjv4L85hsfwgkZo7Nhecr160BE0MrkJNavtllsqm709DVeYvbI_I1WBg",
    },
    {
        "wrong product",
        "IEUM1.eyJleHBpcmVzX2F0IjpudWxsLCJmZWF0dXJlcyI6WyJmaWxlLXRyYW5zZmVyIl0sImlzc3VlZF9hdCI6IjIwMjYt"
        "MDgtMDFUMDA6MDA6MDBaIiwia2V5X2lkIjoidGVzdC1rZXktMjAyNi0wMSIsImxpY2Vuc2VfaWQiOiIxMTExMTExMS0yMjIy"
        "LTQzMzMtODQ0NC01NTU1NTU1NTU1NTUiLCJub3RfYmVmb3JlIjoiMjAyNi0wOC0wMVQwMDowMDowMFoiLCJwcm9kdWN0Ijoi"
        "ZW50ZXJwcmlzZS1jbG91ZCIsInJlY2lwaWVudCI6ImZpeHR1cmUtb3duZXIiLCJzY2hlbWEiOiJpZXVtLnByby1saWNlbnNl"
        "LnYxIn0.WEc5jbC2oa9eY0W1OQVnlKYrujnD9mP6T8E6l7LUi-ErWB29k2-1-eY40G_L7ti_DJPaKu1wGsr7FiwFdfF6CQ",
    },
    {
        "missing file transfer feature",
        "IEUM1.eyJleHBpcmVzX2F0IjpudWxsLCJmZWF0dXJlcyI6WyJjbGlwYm9hcmQtcHJvIl0sImlzc3VlZF9hdCI6IjIwMjYt"
        "MDgtMDFUMDA6MDA6MDBaIiwia2V5X2lkIjoidGVzdC1rZXktMjAyNi0wMSIsImxpY2Vuc2VfaWQiOiIxMTExMTExMS0yMjIy"
        "LTQzMzMtODQ0NC01NTU1NTU1NTU1NTUiLCJub3RfYmVmb3JlIjoiMjAyNi0wOC0wMVQwMDowMDowMFoiLCJwcm9kdWN0Ijoi"
        "cHJvLWxvY2FsIiwicmVjaXBpZW50IjoiZml4dHVyZS1vd25lciIsInNjaGVtYSI6ImlldW0ucHJvLWxpY2Vuc2UudjEifQ."
        "axaeyxGVmfAKB9E6VE-6t4ig3n8hgYlSDZqknIVHyE6O8g4DqSja1hP2cI1O0lOwaYy-BLUA1S32wRXmz_LbCQ",
    },
};

inline constexpr InvalidFixture kMalformedSignedFixtures[] = {
    {
        "malformed uuid",
        "IEUM1.eyJleHBpcmVzX2F0IjpudWxsLCJmZWF0dXJlcyI6WyJmaWxlLXRyYW5zZmVyIl0sImlzc3VlZF9hdCI6IjIwMjYt"
        "MDgtMDFUMDA6MDA6MDBaIiwia2V5X2lkIjoidGVzdC1rZXktMjAyNi0wMSIsImxpY2Vuc2VfaWQiOiJOT1QtQS1VVUlEIiwi"
        "bm90X2JlZm9yZSI6IjIwMjYtMDgtMDFUMDA6MDA6MDBaIiwicHJvZHVjdCI6InByby1sb2NhbCIsInJlY2lwaWVudCI6ImZp"
        "eHR1cmUtb3duZXIiLCJzY2hlbWEiOiJpZXVtLnByby1saWNlbnNlLnYxIn0."
        "QeWiT6U_SnDyUId8MM_UCnkod9hte7JDdxLz-yOOOpVfbWflf7VZinAlmNrjSMMeEHxR2KQhWM1L152B8VCsCQ",
    },
    {
        "bad issued at",
        "IEUM1.eyJleHBpcmVzX2F0IjpudWxsLCJmZWF0dXJlcyI6WyJmaWxlLXRyYW5zZmVyIl0sImlzc3VlZF9hdCI6IjIwMjYt"
        "MDgtMDEgMDA6MDA6MDAiLCJrZXlfaWQiOiJ0ZXN0LWtleS0yMDI2LTAxIiwibGljZW5zZV9pZCI6IjExMTExMTExLTIyMjIt"
        "NDMzMy04NDQ0LTU1NTU1NTU1NTU1Iiwibm90X2JlZm9yZSI6IjIwMjYtMDgtMDFUMDA6MDA6MDBaIiwicHJvZHVjdCI6InBy"
        "by1sb2NhbCIsInJlY2lwaWVudCI6ImZpeHR1cmUtb3duZXIiLCJzY2hlbWEiOiJpZXVtLnByby1saWNlbnNlLnYxIn0."
        "JjZbcsZ36Y4pRAw4UK6EmFKOdtzTcsQzzKasjARGRZJetwSCINpGbEbEt63PU5Vnw8BOmucxErL7VhKb2y_jCw",
    },
    {
        "non UTC not before",
        "IEUM1.eyJleHBpcmVzX2F0IjpudWxsLCJmZWF0dXJlcyI6WyJmaWxlLXRyYW5zZmVyIl0sImlzc3VlZF9hdCI6IjIwMjYt"
        "MDgtMDFUMDA6MDA6MDBaIiwia2V5X2lkIjoidGVzdC1rZXktMjAyNi0wMSIsImxpY2Vuc2VfaWQiOiIxMTExMTExMS0yMjIy"
        "LTQzMzMtODQ0NC01NTU1NTU1NTU1NTUiLCJub3RfYmVmb3JlIjoiMjAyNi0wOC0wMVQwMDowMDowMCswMDowMCIsInByb2R1"
        "Y3QiOiJwcm8tbG9jYWwiLCJyZWNpcGllbnQiOiJmaXh0dXJlLW93bmVyIiwic2NoZW1hIjoiaWV1bS5wcm8tbGljZW5zZS52"
        "MSJ9.f9DZ72Ut4SxU2PIyuDvv-zFACRCk2I6--FVkymj7UZjXT73muVXyolF1a5ajlOhVQ7Cf9gvIRCUN3b7DhuJkCQ",
    },
    {
        "bad expiry",
        "IEUM1.eyJleHBpcmVzX2F0IjoidG9tb3Jyb3ciLCJmZWF0dXJlcyI6WyJmaWxlLXRyYW5zZmVyIl0sImlzc3VlZF9hdCI6"
        "IjIwMjYtMDgtMDFUMDA6MDA6MDBaIiwia2V5X2lkIjoidGVzdC1rZXktMjAyNi0wMSIsImxpY2Vuc2VfaWQiOiIxMTExMTEx"
        "MS0yMjIyLTQzMzMtODQ0NC01NTU1NTU1NTU1NTUiLCJub3RfYmVmb3JlIjoiMjAyNi0wOC0wMVQwMDowMDowMFoiLCJwcm9k"
        "dWN0IjoicHJvLWxvY2FsIiwicmVjaXBpZW50IjoiZml4dHVyZS1vd25lciIsInNjaGVtYSI6ImlldW0ucHJvLWxpY2Vuc2Uu"
        "djEifQ.U3Svi6raWFNx8DvC9AVfQXZGXTuCmJmbU47T9n0zLWrAC6BGCLIwCPc60P9M_ZfQtPhLK8zn8ieYolQKWHQ_CA",
    },
    {
        "wrong feature type",
        "IEUM1.eyJleHBpcmVzX2F0IjpudWxsLCJmZWF0dXJlcyI6ImZpbGUtdHJhbnNmZXIiLCJpc3N1ZWRfYXQiOiIyMDI2LTA4"
        "LTAxVDAwOjAwOjAwWiIsImtleV9pZCI6InRlc3Qta2V5LTIwMjYtMDEiLCJsaWNlbnNlX2lkIjoiMTExMTExMTEtMjIyMi00"
        "MzMzLTg0NDQtNTU1NTU1NTU1NTU1Iiwibm90X2JlZm9yZSI6IjIwMjYtMDgtMDFUMDA6MDA6MDBaIiwicHJvZHVjdCI6InBy"
        "by1sb2NhbCIsInJlY2lwaWVudCI6ImZpeHR1cmUtb3duZXIiLCJzY2hlbWEiOiJpZXVtLnByby1saWNlbnNlLnYxIn0."
        "_VMrW_3vdMmjBpRinS1SQUHwYM2gk_FT6zkMoBypDGmAUj7moK99PeIoBqfaDQpaO9lPIqrErzv73YqpyFnRBw",
    },
    {
        "missing recipient",
        "IEUM1.eyJleHBpcmVzX2F0IjpudWxsLCJmZWF0dXJlcyI6WyJmaWxlLXRyYW5zZmVyIl0sImlzc3VlZF9hdCI6IjIwMjYt"
        "MDgtMDFUMDA6MDA6MDBaIiwia2V5X2lkIjoidGVzdC1rZXktMjAyNi0wMSIsImxpY2Vuc2VfaWQiOiIxMTExMTExMS0yMjIy"
        "LTQzMzMtODQ0NC01NTU1NTU1NTU1NTUiLCJub3RfYmVmb3JlIjoiMjAyNi0wOC0wMVQwMDowMDowMFoiLCJwcm9kdWN0Ijoi"
        "cHJvLWxvY2FsIiwic2NoZW1hIjoiaWV1bS5wcm8tbGljZW5zZS52MSJ9."
        "VNltN4EtjkbMNbrBrOE6pt1163ZKLMykwl-Ph4zwzPzGFfcYZsLEGslSuBFai8oimk1XEyAQ87Q7cAhkUajxCQ",
    },
    {
        "malformed json",
        "IEUM1.e25vdC1qc29u.fp4I8LBsbX4jM-s-u7cL5flQkdlD_PcCuaPxTpRC2lA2OeOl_BOvAGMI4tOK3Zbx0QJWLI4bQKyumkEvyHrZDg",
    },
    {
        "array root",
        "IEUM1.W10.b1Uoyd_L0zyQQgqpeFmzEIzWl8Af6PwUsnSm9nBz1bLiXqKLq2rCX0d_4kJFm2ZikZSiG_43xMdKavY2JUTnDA",
    },
};

} // namespace deskflow::licensing::test_fixture
