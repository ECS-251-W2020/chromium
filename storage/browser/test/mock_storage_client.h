// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_MOCK_STORAGE_CLIENT_H_
#define STORAGE_BROWSER_TEST_MOCK_STORAGE_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "storage/browser/quota/quota_client.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace storage {

class QuotaManagerProxy;

struct MockOriginData {
  const char* origin;
  blink::mojom::StorageType type;
  int64_t usage;
};

// Mock QuotaClient implementation for testing.
class MockStorageClient : public QuotaClient {
 public:
  MockStorageClient(scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
                    const MockOriginData* mock_data,
                    QuotaClient::ID id,
                    size_t mock_data_size);

  // To add or modify mock data in this client.
  void AddOriginAndNotify(const url::Origin& origin,
                          blink::mojom::StorageType type,
                          int64_t size);
  void ModifyOriginAndNotify(const url::Origin& origin,
                             blink::mojom::StorageType type,
                             int64_t delta);
  void TouchAllOriginsAndNotify();

  void AddOriginToErrorSet(const url::Origin& origin,
                           blink::mojom::StorageType type);

  base::Time IncrementMockTime();

  // QuotaClient.
  QuotaClient::ID id() const override;
  void OnQuotaManagerDestroyed() override;
  void GetOriginUsage(const url::Origin& origin,
                      blink::mojom::StorageType type,
                      GetUsageCallback callback) override;
  void GetOriginsForType(blink::mojom::StorageType type,
                         GetOriginsCallback callback) override;
  void GetOriginsForHost(blink::mojom::StorageType type,
                         const std::string& host,
                         GetOriginsCallback callback) override;
  void DeleteOriginData(const url::Origin& origin,
                        blink::mojom::StorageType type,
                        DeletionCallback callback) override;
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             base::OnceClosure callback) override;
  bool DoesSupport(blink::mojom::StorageType type) const override;

 private:
  ~MockStorageClient() override;

  void RunGetOriginUsage(const url::Origin& origin,
                         blink::mojom::StorageType type,
                         GetUsageCallback callback);
  void RunGetOriginsForType(blink::mojom::StorageType type,
                            GetOriginsCallback callback);
  void RunGetOriginsForHost(blink::mojom::StorageType type,
                            const std::string& host,
                            GetOriginsCallback callback);
  void RunDeleteOriginData(const url::Origin& origin,
                           blink::mojom::StorageType type,
                           DeletionCallback callback);

  void Populate(const MockOriginData* mock_data, size_t mock_data_size);

  const scoped_refptr<QuotaManagerProxy> quota_manager_proxy_;
  const ID id_;

  std::map<std::pair<url::Origin, blink::mojom::StorageType>, int64_t>
      origin_data_;
  std::set<std::pair<url::Origin, blink::mojom::StorageType>> error_origins_;

  int mock_time_counter_;

  base::WeakPtrFactory<MockStorageClient> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MockStorageClient);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_MOCK_STORAGE_CLIENT_H_
