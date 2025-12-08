/* 
   Multithreaded Web Server Demo - JavaScript Test Suite
*/

// Global state
let totalRequests = 0;
const resultsLog = document.getElementById('results-log');

// LOGGING UTILITIES
function getTimestamp() {
    const now = new Date();
    return now.toLocaleTimeString('en-US', { hour12: false });
}

function log(message, type = 'info') {
    const entry = document.createElement('div');
    entry.className = `log-entry ${type}`;
    entry.innerHTML = `<span class="log-time">[${getTimestamp()}]</span>${message}`;
    resultsLog.appendChild(entry);
    resultsLog.scrollTop = resultsLog.scrollHeight;
}

function clearResults() {
    resultsLog.innerHTML = '<div class="log-entry info">Results cleared. Ready for new tests...</div>';
}

function updateRequestCount() {
    document.getElementById('requests-value').textContent = totalRequests;
}

// CONNECTION STATUS CHECK
async function checkConnection() {
    const statusCard = document.getElementById('connection-status');
    const statusValue = document.getElementById('conn-value');
    const statusIcon = statusCard.querySelector('.status-icon');
    
    try {
        const start = performance.now();
        const response = await fetch('/index.html', { cache: 'no-store' });
        const latency = Math.round(performance.now() - start);
        
        if (response.ok) {
            statusCard.className = 'status-card connected';
            statusIcon.textContent = '✅';
            statusValue.textContent = 'Connected';
            document.getElementById('latency-value').textContent = `${latency}ms`;
            log('Server connection verified', 'success');
            return true;
        }
    } catch (error) {
        statusCard.className = 'status-card error';
        statusIcon.textContent = '❌';
        statusValue.textContent = 'Disconnected';
        log(`Connection failed: ${error.message}`, 'error');
    }
    return false;
}

// FILE SERVING TESTS
async function testFile(filename, expectedMime) {
    log(`Testing file: ${filename}`, 'info');
    totalRequests++;
    updateRequestCount();
    
    try {
        const start = performance.now();
        const response = await fetch(`/${filename}`, { cache: 'no-store' });
        const latency = Math.round(performance.now() - start);
        
        const contentType = response.headers.get('Content-Type') || 'unknown';
        const status = response.status;
        
        if (status === 200) {
            const body = await response.text();
            const size = body.length;
            
            if (contentType.includes(expectedMime)) {
                log(`✅ ${filename}: HTTP ${status}, ${contentType}, ${size} bytes, ${latency}ms`, 'success');
            } else {
                log(`⚠️ ${filename}: HTTP ${status}, Expected ${expectedMime} but got ${contentType}`, 'warning');
            }
        } else {
            log(`❌ ${filename}: HTTP ${status}`, 'error');
        }
    } catch (error) {
        log(`❌ ${filename}: ${error.message}`, 'error');
    }
}

// ERROR HANDLING TESTS
async function test404() {
    const filename = 'nonexistent_file_' + Date.now() + '.html';
    log(`Testing 404 error: ${filename}`, 'info');
    totalRequests++;
    updateRequestCount();
    
    try {
        const response = await fetch(`/${filename}`, { cache: 'no-store' });
        
        if (response.status === 404) {
            log(`✅ 404 Test: Server correctly returned HTTP 404`, 'success');
        } else {
            log(`❌ 404 Test: Expected 404, got HTTP ${response.status}`, 'error');
        }
    } catch (error) {
        log(`❌ 404 Test failed: ${error.message}`, 'error');
    }
}

async function testDeepPath() {
    const path = '/fake/deep/nested/path/file.html';
    log(`Testing deep path 404: ${path}`, 'info');
    totalRequests++;
    updateRequestCount();
    
    try {
        const response = await fetch(path, { cache: 'no-store' });
        
        if (response.status === 404) {
            log(`✅ Deep Path Test: Server correctly returned HTTP 404`, 'success');
        } else {
            log(`❌ Deep Path Test: Expected 404, got HTTP ${response.status}`, 'error');
        }
    } catch (error) {
        log(`❌ Deep Path Test failed: ${error.message}`, 'error');
    }
}

// CONCURRENCY TESTS
async function testConcurrent(count) {
    log(`Starting ${count} concurrent requests...`, 'info');
    
    const start = performance.now();
    const promises = [];
    
    for (let i = 0; i < count; i++) {
        totalRequests++;
        promises.push(
            fetch('/index.html', { cache: 'no-store' })
                .then(r => ({ success: r.ok, status: r.status }))
                .catch(e => ({ success: false, error: e.message }))
        );
    }
    
    updateRequestCount();
    
    const results = await Promise.all(promises);
    const elapsed = Math.round(performance.now() - start);
    
    const successful = results.filter(r => r.success).length;
    const failed = count - successful;
    
    if (failed === 0) {
        log(`✅ Concurrent Test: ${successful}/${count} requests succeeded in ${elapsed}ms`, 'success');
    } else {
        log(`⚠️ Concurrent Test: ${successful}/${count} succeeded, ${failed} failed in ${elapsed}ms`, 'warning');
    }
    
    // Show throughput
    const throughput = Math.round((count / elapsed) * 1000);
    log(`   Throughput: ~${throughput} requests/second`, 'info');
}

async function testSequential(count) {
    log(`Starting ${count} sequential requests...`, 'info');
    
    const start = performance.now();
    let successful = 0;
    let latencies = [];
    
    for (let i = 0; i < count; i++) {
        totalRequests++;
        updateRequestCount();
        
        try {
            const reqStart = performance.now();
            const response = await fetch('/index.html', { cache: 'no-store' });
            latencies.push(performance.now() - reqStart);
            
            if (response.ok) successful++;
        } catch (error) {
            // Count as failed
        }
    }
    
    const elapsed = Math.round(performance.now() - start);
    const avgLatency = Math.round(latencies.reduce((a, b) => a + b, 0) / latencies.length);
    
    if (successful === count) {
        log(`✅ Sequential Test: ${successful}/${count} requests succeeded in ${elapsed}ms`, 'success');
    } else {
        log(`⚠️ Sequential Test: ${successful}/${count} succeeded in ${elapsed}ms`, 'warning');
    }
    
    log(`   Average latency: ${avgLatency}ms per request`, 'info');
}

// PERFORMANCE TESTS
async function testLatency(count) {
    log(`Measuring latency over ${count} requests...`, 'info');
    
    const latencies = [];
    
    for (let i = 0; i < count; i++) {
        totalRequests++;
        updateRequestCount();
        
        try {
            const start = performance.now();
            await fetch('/index.html', { cache: 'no-store' });
            latencies.push(performance.now() - start);
        } catch (error) {
            log(`Request ${i + 1} failed: ${error.message}`, 'error');
        }
    }
    
    if (latencies.length > 0) {
        const min = Math.round(Math.min(...latencies));
        const max = Math.round(Math.max(...latencies));
        const avg = Math.round(latencies.reduce((a, b) => a + b, 0) / latencies.length);
        
        log(`✅ Latency Results:`, 'success');
        log(`   Min: ${min}ms | Avg: ${avg}ms | Max: ${max}ms`, 'info');
        
        document.getElementById('latency-value').textContent = `${avg}ms`;
    }
}

async function stressTest() {
    log(`🔥 Starting stress test (50 rapid requests)...`, 'warning');
    
    const start = performance.now();
    const batchSize = 10;
    const batches = 5;
    let totalSuccess = 0;
    let totalFailed = 0;
    
    for (let batch = 0; batch < batches; batch++) {
        const promises = [];
        
        for (let i = 0; i < batchSize; i++) {
            totalRequests++;
            promises.push(
                fetch('/index.html', { cache: 'no-store' })
                    .then(r => r.ok)
                    .catch(() => false)
            );
        }
        
        updateRequestCount();
        const results = await Promise.all(promises);
        
        totalSuccess += results.filter(r => r).length;
        totalFailed += results.filter(r => !r).length;
        
        log(`   Batch ${batch + 1}/${batches}: ${results.filter(r => r).length}/${batchSize} succeeded`, 'info');
    }
    
    const elapsed = Math.round(performance.now() - start);
    const throughput = Math.round((50 / elapsed) * 1000);
    
    if (totalFailed === 0) {
        log(`✅ Stress Test Complete: 50/50 requests in ${elapsed}ms (~${throughput} req/s)`, 'success');
    } else {
        log(`⚠️ Stress Test: ${totalSuccess}/50 succeeded, ${totalFailed} failed in ${elapsed}ms`, 'warning');
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

document.addEventListener('DOMContentLoaded', () => {
    log('Demo page loaded. Checking server connection...', 'info');
    checkConnection();
});

// Periodic connection check
setInterval(checkConnection, 30000);
