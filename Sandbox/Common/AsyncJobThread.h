#pragma once

template< typename Job >
class AsyncJobThread
{
protected:
  using ProcessFn = eastl::function< void( Job& ) >;

  AsyncJobThread()
  {
    InitializeCriticalSectionAndSpinCount( &queueLock, 4000 );
    hasWork = CreateEventA( nullptr, false, false, nullptr );
  }

  ~AsyncJobThread()
  {
    EnterCriticalSection( &queueLock );
    while ( !jobs.empty() )
      jobs.pop();
    LeaveCriticalSection( &queueLock );

    keepWorking = false;
    SetEvent( hasWork );
    if ( workerThread.joinable() )
      workerThread.join();
    DeleteCriticalSection( &queueLock );
  }

  void Start( ProcessFn fn, const char* name )
  {
    this->fn = fn;

    workerThread = std::thread( WorkerThreadFunc, eastl::ref( *this ), name );
  }

  void Enqueue( Job&& job )
  {
    EnterCriticalSection( &queueLock );
    jobs.emplace( eastl::forward< Job >( job ) );
    LeaveCriticalSection( &queueLock );

    SetEvent( hasWork );
  }

private:
  static void WorkerThreadFunc( AsyncJobThread& thiz, const char* name )
  {
    SetThreadName( GetCurrentThreadId(), (char*)name );

    while ( thiz.keepWorking )
    {
      WaitForSingleObject( thiz.hasWork, INFINITE );

      while ( !thiz.jobs.empty() )
      {
        EnterCriticalSection( &thiz.queueLock );
        auto job = eastl::move( thiz.jobs.front() );
        thiz.jobs.pop();
        LeaveCriticalSection( &thiz.queueLock );

        thiz.fn( job );
      }
    }
  }

  eastl::atomic< bool > keepWorking = true;
  eastl::queue< Job >   jobs;

  CRITICAL_SECTION queueLock;
  HANDLE hasWork = INVALID_HANDLE_VALUE;

  std::thread workerThread;

  ProcessFn fn;
};