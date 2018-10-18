using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class purchase_infoViewModel
    {
        public long rad_tr_id { get; set; }
        public long trn_id { get; set; }
        public string json_text { get; set; }
        public long? request_type { get; set; }
        public string input_of_request { get; set; }
    }
}