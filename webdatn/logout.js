document.addEventListener('DOMContentLoaded', function() {
  // Tìm mục Đăng xuất
  const logoutBtn = document.getElementById('logoutBtn');
  
  if (logoutBtn) {
    logoutBtn.addEventListener('click', function(event) {
      event.preventDefault(); // Ngăn hành vi mặc định của thẻ <a>
      
      // Xóa thông tin phiên
      localStorage.removeItem('token'); // Xóa token nếu dùng localStorage
      sessionStorage.removeItem('token'); // Xóa token nếu dùng sessionStorage

      // Chuyển hướng về trang login.html
      window.location.href = 'login.html';
    });
  }

  // Kiểm tra trạng thái đăng nhập (tùy chọn)
  // Nếu không có token, chuyển hướng về login.html
  if (!localStorage.getItem('token') && !sessionStorage.getItem('token')) {
    if (window.location.pathname !== '/login.html') {
      window.location.href = 'login.html';
    }
  }
});